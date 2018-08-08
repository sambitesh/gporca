//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2004-2015 Pivotal Software, Inc.
//
//	@filename:
//		CWorkerPoolManager.cpp
//
//	@doc:
//		Central scheduler;
//		* maintains global worker-local-storage
//		* keeps track of all worker pools
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/common/CAutoP.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/error/CFSimulator.h"  // for GPOS_FPSIMULATOR
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/IMemoryPool.h"
#include "gpos/memory/CMemoryPoolAlloc.h"
#include "gpos/memory/CMemoryPoolInjectFault.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/memory/CMemoryPoolStack.h"
#include "gpos/memory/CMemoryPoolTracker.h"
#include "gpos/memory/CMemoryVisitorPrint.h"
#include "gpos/sync/CAutoSpinlock.h"
#include "gpos/sync/CAutoMutex.h"
#include "gpos/task/CAutoSuspendAbort.h"


using namespace gpos;
using namespace gpos::clib;


// global instance of memory pool manager
CMemoryPoolManager *CMemoryPoolManager::m_mp_mgr = NULL;

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::CMemoryPoolManager
//
//	@doc:
//		Ctor.
//
//---------------------------------------------------------------------------
CMemoryPoolManager::CMemoryPoolManager(IMemoryPool *internal, IMemoryPool *base)
	: m_base_mp(base),
	  m_internal_mp(internal),
	  m_global_mp(NULL),
	  m_allow_global_new(true)
{
	GPOS_ASSERT(NULL != internal);
	GPOS_ASSERT(NULL != base);
	GPOS_ASSERT(GPOS_OFFSET(CMemoryPool, m_link) == GPOS_OFFSET(CMemoryPoolAlloc, m_link));
	GPOS_ASSERT(GPOS_OFFSET(CMemoryPool, m_link) == GPOS_OFFSET(CMemoryPoolTracker, m_link));

	m_hash_table.Init(m_internal_mp,
					  GPOS_MEMORY_POOL_HT_SIZE,
					  GPOS_OFFSET(CMemoryPool, m_link),
					  GPOS_OFFSET(CMemoryPool, m_hash_key),
					  &(CMemoryPool::m_invalid),
					  HashULongPtr,
					  EqualULongPtr);

	// create pool used in allocations made using global new operator
	m_global_mp = Create(EatTracker, true, gpos::ullong_max);
}

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Init
//
//	@doc:
//		Initializer for global memory pool manager
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolManager::Init(void *(*alloc)(SIZE_T), void (*free_func)(void *))
{
	GPOS_ASSERT(NULL == CMemoryPoolManager::m_mp_mgr);

	// raw allocation of memory for internal memory pools
	void *alloc_base = Malloc(sizeof(CMemoryPoolAlloc));
	void *alloc_internal = Malloc(sizeof(CMemoryPoolTracker));

	// check if any allocation failed
	if (NULL == alloc_internal || NULL == alloc_base)
	{
		Free(alloc_base);
		Free(alloc_internal);

		return GPOS_OOM;
	}

	// create base memory pool
	IMemoryPool *base = new (alloc_base) CMemoryPoolAlloc(alloc, free_func);

	// create internal memory pool
	IMemoryPool *internal =
		new (alloc_internal) CMemoryPoolTracker(base,
												gpos::ullong_max,  // ullMaxMemory
												true,			   // IsThreadSafe
												false			   //fOwnsUnderlyingPmp
		);

	// instantiate manager
	GPOS_TRY
	{
		CMemoryPoolManager::m_mp_mgr =
			GPOS_NEW(internal) CMemoryPoolManager(internal, base);
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM))
		{
			Free(alloc_base);
			Free(alloc_internal);

			return GPOS_OOM;
		}

		return GPOS_FAILED;
	}
	GPOS_CATCH_END;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Create
//
//	@doc:
//		Create new memory pool
//
//---------------------------------------------------------------------------
IMemoryPool *
CMemoryPoolManager::Create(AllocType alloc_type, BOOL thread_safe, ULLONG capacity)
{
	IMemoryPool *mp =
#ifdef GPOS_DEBUG
		CreatePoolStack(alloc_type, capacity, thread_safe);
#else
		New(alloc_type,
			m_base_mp,
			capacity,
			thread_safe,
			false /*owns_underlying_mp*/);
#endif  // GPOS_DEBUG

	// accessor scope
	{
		MemoryPoolKeyAccessor acc(m_hash_table, mp->GetHashKey());
		acc.Insert(Convert(mp));
	}

	return mp;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::New
//
//	@doc:
//		Create new pool of given type
//
//---------------------------------------------------------------------------
IMemoryPool *
CMemoryPoolManager::New(AllocType alloc_type,
						IMemoryPool *underlying_mp,
						ULLONG capacity,
						BOOL thread_safe,
						BOOL owns_underlying_mp)
{
	switch (alloc_type)
	{
		case CMemoryPoolManager::EatTracker:
			return GPOS_NEW(m_internal_mp) CMemoryPoolTracker(
				underlying_mp, capacity, thread_safe, owns_underlying_mp);

		case CMemoryPoolManager::EatStack:
			return GPOS_NEW(m_internal_mp) CMemoryPoolStack(
				underlying_mp, capacity, thread_safe, owns_underlying_mp);
	}

	GPOS_ASSERT(!"No matching pool type found");
	return NULL;
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::CreatePoolStack
//
//	@doc:
//		Surround new pool with tracker pools
//
//---------------------------------------------------------------------------
IMemoryPool *
CMemoryPoolManager::CreatePoolStack(AllocType alloc_type, ULLONG capacity, BOOL thread_safe)
{
	IMemoryPool *base = m_base_mp;
	BOOL malloc_type = (EatTracker == alloc_type);

	// check if tracking and fault injection on internal allocations
	// of memory pools is enabled
	if (NULL != ITask::Self() && !malloc_type && GPOS_FTRACE(EtraceTestMemoryPools))
	{
		// put fault injector on top of base pool
		IMemoryPool *FPSim_low = GPOS_NEW(m_internal_mp)
			CMemoryPoolInjectFault(base, false /*owns_underlying_mp*/
			);

		// put tracker on top of fault injector
		base =
			New(EatTracker, FPSim_low, capacity, thread_safe, true /*owns_underlying_mp*/
			);
	}

	// tracker pool goes on top
	IMemoryPool *requested = base;
	if (!malloc_type)
	{
		// put requested pool on top of underlying pool
		requested = New(alloc_type, base, capacity, thread_safe, base != m_base_mp);
	}

	// put fault injector on top of requested pool
	IMemoryPool *FPSim =
		GPOS_NEW(m_internal_mp) CMemoryPoolInjectFault(requested, !malloc_type);

	// put tracker on top of the stack
	return New(EatTracker, FPSim, capacity, thread_safe, true /*fOwnsUnderlying*/);
}

#endif  // GPOS_DEBUG



//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::DeleteUnregistered
//
//	@doc:
//		Release returned pool
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::DeleteUnregistered(IMemoryPool *mp)
{
	GPOS_ASSERT(mp != NULL);

#ifdef GPOS_DEBUG
	// accessor's scope
	{
		MemoryPoolKeyAccessor acc(m_hash_table, mp->GetHashKey());

		// make sure that this pool is not in the hash table
		IMemoryPool *found = acc.Find();
		while (NULL != found)
		{
			GPOS_ASSERT(found != mp && "Attempt to delete a registered memory pool");

			found = acc.Next(Convert(found));
		}
	}
#endif  // GPOS_DEBUG

	GPOS_DELETE(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Destroy
//
//	@doc:
//		Release returned pool
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::Destroy(IMemoryPool *mp)
{
	GPOS_ASSERT(NULL != mp);

	// accessor scope
	{
		MemoryPoolKeyAccessor acc(m_hash_table, mp->GetHashKey());
		acc.Remove(Convert(mp));
	}

	mp->TearDown();

	GPOS_DELETE(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::TotalAllocatedSize
//
//	@doc:
//		Return total allocated size in bytes
//
//---------------------------------------------------------------------------
ULLONG
CMemoryPoolManager::TotalAllocatedSize()
{
	ULLONG total_size = 0;
	MemoryPoolIter iter(m_hash_table);
	while (iter.Advance())
	{
		MemoryPoolIterAccessor acc(iter);
		IMemoryPool *mp = acc.Value();
		if (NULL != mp)
		{
			total_size = total_size + mp->TotalAllocatedSize();
		}
	}

	return total_size;
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::OsPrint
//
//	@doc:
//		Print contents of all allocated memory pools
//
//---------------------------------------------------------------------------
IOstream &
CMemoryPoolManager::OsPrint(IOstream &os)
{
	os << "Print memory pools: " << std::endl;

	MemoryPoolIter iter(m_hash_table);
	while (iter.Advance())
	{
		IMemoryPool *mp = NULL;
		{
			MemoryPoolIterAccessor acc(iter);
			mp = acc.Value();
		}

		if (NULL != mp)
		{
			os << *mp << std::endl;
		}
	}

	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::PrintOverSizedPools
//
//	@doc:
//		Print memory pools with total allocated size above given threshold
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::PrintOverSizedPools(IMemoryPool *trace,
										ULLONG size_threshold  // size threshold in bytes
)
{
	CAutoTraceFlag Abort(EtraceSimulateAbort, false);
	CAutoTraceFlag OOM(EtraceSimulateOOM, false);
	CAutoTraceFlag Net(EtraceSimulateNetError, false);
	CAutoTraceFlag IO(EtraceSimulateIOError, false);

	MemoryPoolIter iter(m_hash_table);
	while (iter.Advance())
	{
		MemoryPoolIterAccessor acc(iter);
		IMemoryPool *mp = acc.Value();

		if (NULL != mp)
		{
			ULLONG size = mp->TotalAllocatedSize();
			if (size > size_threshold)
			{
				CAutoTrace at(trace);
				at.Os() << std::endl << "OVERSIZED MEMORY POOL: " << size << " bytes " << std::endl;
			}
		}
	}
}
#endif  // GPOS_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::DestroyMemoryPoolAtShutdown
//
//	@doc:
//		Destroy a memory pool at shutdown
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::DestroyMemoryPoolAtShutdown(CMemoryPool *mp)
{
	GPOS_ASSERT(NULL != mp);

#ifdef GPOS_DEBUG
	gpos::oswcerr << "Leaked " << *mp << std::endl;
#endif  // GPOS_DEBUG

	mp->TearDown();
	GPOS_DELETE(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Cleanup
//
//	@doc:
//		Clean-up memory pools
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::Cleanup()
{
#ifdef GPOS_DEBUG
	if (0 < m_global_mp->TotalAllocatedSize())
	{
		// allocations made by calling global new operator are not deleted
		gpos::oswcerr << "Memory leaks detected" << std::endl << *m_global_mp << std::endl;
	}
#endif  // GPOS_DEBUG

	GPOS_ASSERT(NULL != m_global_mp);
	Destroy(m_global_mp);

	// cleanup left-over memory pools;
	// any such pool means that we have a leak
	m_hash_table.DestroyEntries(DestroyMemoryPoolAtShutdown);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolManager::Shutdown
//
//	@doc:
//		Delete memory pools and release manager
//
//---------------------------------------------------------------------------
void
CMemoryPoolManager::Shutdown()
{
	// cleanup remaining memory pools
	Cleanup();

	// save off pointers for explicit deletion
	IMemoryPool *internal = m_internal_mp;
	IMemoryPool *base = m_base_mp;

	GPOS_DELETE(CMemoryPoolManager::m_mp_mgr);
	CMemoryPoolManager::m_mp_mgr = NULL;

#ifdef GPOS_DEBUG
	internal->AssertEmpty(oswcerr);
	base->AssertEmpty(oswcerr);
#endif  // GPOS_DEBUG

	Free(internal);
	Free(base);
}

// EOF

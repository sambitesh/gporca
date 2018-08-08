//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009-2010 Greenplum Inc.
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMemoryPoolTracker.h
//
//	@doc:
//		Memory pool that allocates from an underlying allocator and adds on
//		statistics and debugging
//
//	@owner:
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOS_CMemoryPoolTracker_H
#define GPOS_CMemoryPoolTracker_H

#include "gpos/assert.h"
#include "gpos/types.h"
#include "gpos/utils.h"
#include "gpos/common/CStackDescriptor.h"
#include "gpos/memory/CMemoryPool.h"
#include "gpos/sync/CAutoSpinlock.h"
#include "gpos/sync/CSpinlock.h"

namespace gpos
{
	// prototypes
	class CAutoMutex;

	// memory pool with statistics and debugging support
	class CMemoryPoolTracker : public CMemoryPool
	{
	private:
		//---------------------------------------------------------------------------
		//	@struct:
		//		AllocHeader
		//
		//	@doc:
		//		Defines memory block header layout for all allocations;
		//	 	does not include the pointer to the pool;
		//
		//---------------------------------------------------------------------------
		struct SAllocHeader
		{
			// sequence number
			ULLONG m_serial;

			// user-visible size
			ULONG m_size;

			// file name
			const CHAR *m_filename;

			// line in file
			ULONG m_line;

#ifdef GPOS_DEBUG
			// allocation stack
			CStackDescriptor m_stack_desc;
#endif  // GPOS_DEBUG

			// link for allocation list
			SLink m_link;
		};

		// lock for synchronization
		CSpinlockOS m_lock;

		// statistics
		CMemoryPoolStatistics m_mp_statistics;

		// allocation sequence number
		ULONG m_alloc_sequence;

		// memory pool capacity;
		// if equal to ULLONG, checks for exceeding max memory are bypassed
		const ULLONG m_capacity;

		// size of reserved memory;
		// this includes total allocated memory and pending allocations;
		ULLONG m_reserved;

		// list of allocated (live) objects
		CList<SAllocHeader> m_allocations_list;

		// attempt to reserve memory for allocation
		BOOL Reserve(CAutoSpinlock &as, ULONG ulAlloc);

		// revert memory reservation
		void Unreserve(CAutoSpinlock &as, ULONG alloc, BOOL mem_available);

		// acquire spinlock if pool is thread-safe
		void
		SLock(CAutoSpinlock &as)
		{
			if (IsThreadSafe())
			{
				as.Lock();
			}
		}

		// release spinlock if pool is thread-safe
		void
		SUnlock(CAutoSpinlock &as)
		{
			if (IsThreadSafe())
			{
				as.Unlock();
			}
		}

		// private copy ctor
		CMemoryPoolTracker(CMemoryPoolTracker &);

	protected:
		// dtor
		virtual ~CMemoryPoolTracker();

	public:
		// ctor
		CMemoryPoolTracker(IMemoryPool *underlying_mp,
						   ULLONG size,
						   BOOL thread_safe,
						   BOOL owns_underlying_mp);

		// allocate memory
		virtual void *Allocate(const ULONG bytes, const CHAR *file, const ULONG line);

		// free memory
		virtual void Free(void *ptr);

		// prepare the memory pool to be deleted
		virtual void TearDown();

		// check if the pool stores a pointer to itself at the end of
		// the header of each allocated object;
		virtual BOOL
		StoresPoolPointer() const
		{
			return true;
		}

		// return total allocated size
		virtual ULLONG
		TotalAllocatedSize() const
		{
			return m_mp_statistics.TotalAllocatedSize();
		}

#ifdef GPOS_DEBUG

		// check if the memory pool keeps track of live objects
		virtual BOOL
		SupportsLiveObjectWalk() const
		{
			return true;
		}

		// walk the live objects
		virtual void WalkLiveObjects(gpos::IMemoryVisitor *visitor);

		// check if statistics tracking is supported
		virtual BOOL
		SupportsStatistics() const
		{
			return true;
		}

		// return the current statistics
		virtual void UpdateStatistics(CMemoryPoolStatistics &mp_statistics);

#endif  // GPOS_DEBUG
	};
}  // namespace gpos

#endif  // !GPOS_CMemoryPoolTracker_H

// EOF

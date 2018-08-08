//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CPartFilter
//
//	@doc:
//		Partitioned table filter map used in required and derived properties
//---------------------------------------------------------------------------
#ifndef GPOPT_CPartFilterMap_H
#define GPOPT_CPartFilterMap_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CHashMap.h"
#include "gpopt/base/CDrvdProp.h"


namespace gpnaucrates
{
	// forward declarations
	class IStatistics;
}

using gpnaucrates::IStatistics;

namespace gpopt
{

	// forward declarations
	class CExpression;

	//---------------------------------------------------------------------------
	//	@class:
	//		CPartFilterMap
	//
	//	@doc:
	//		Partitioned table filter map used in required and derived properties
	//
	//---------------------------------------------------------------------------
	class CPartFilterMap : public CRefCount
	{
		private:

			//-------------------------------------------------------------------
			//	@class:
			//		CPartFilter
			//
			//	@doc:
			//		Single entry of CPartFilterMap
			//
			//-------------------------------------------------------------------
			class CPartFilter : public CRefCount
			{
				private:

					// scan id
					ULONG m_scan_id;

					// scalar expression
					CExpression *m_pexpr;

					// statistics of the plan below partition selector -- used only during plan property derivation
					IStatistics *m_pstats;

				public:

					// ctor
					CPartFilter
						(
						ULONG scan_id,
						CExpression *pexpr,
						IStatistics *stats = NULL
						);

					// dtor
					virtual
					~CPartFilter();

					// match function
					BOOL Matches(const CPartFilter *ppf) const;

					// return scan id
					ULONG ScanId() const
					{
						return m_scan_id;
					}

					// return scalar expression
					CExpression *Pexpr() const
					{
						return m_pexpr;
					}

					// return statistics of the plan below partition selector
					IStatistics *Pstats() const
					{
						return m_pstats;
					}

					// print function
					IOstream &OsPrint(IOstream &os) const;

			}; // class CPartFilter

			// map of partition index ids to filter expressions
			typedef CHashMap<ULONG, CPartFilter, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
				CleanupDelete<ULONG>, CleanupRelease<CPartFilter> > UlongToPartFilterMap;

			// map iterator
			typedef CHashMapIter<ULONG, CPartFilter, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
				CleanupDelete<ULONG>, CleanupRelease<CPartFilter> > UlongToPartFilterMapIter;

			// hash map from ScanId to CPartFilter
			UlongToPartFilterMap *m_phmulpf;

		public:

			// ctors
			explicit
			CPartFilterMap(IMemoryPool *mp);

			CPartFilterMap(IMemoryPool *mp, CPartFilterMap *ppfm);

			// dtor
			virtual
			~CPartFilterMap();

			// check whether map contains the given scan id
			BOOL FContainsScanId
				(
				ULONG scan_id
				)
				const
			{
				return (NULL != m_phmulpf->Find(&scan_id));
			}

			// the expression associated with the given scan id
			CExpression *Pexpr(ULONG scan_id) const;

			// stats associated with the given scan id
			IStatistics *Pstats(ULONG scan_id) const;

			// check whether the map is empty
			BOOL IsEmpty() const
			{
				return 0 == m_phmulpf->Size();
			}

			// check whether current part filter map is a subset of the given one
			BOOL FSubset(CPartFilterMap *ppfm);

			// check equality of part filter maps
			BOOL Equals
				(
				CPartFilterMap *ppfm
				)
			{
				GPOS_ASSERT(NULL != ppfm);

				return
					(m_phmulpf->Size() == ppfm->m_phmulpf->Size()) &&
					this->FSubset(ppfm);
			}

			// extract part Scan id's in the given memory pool
			ULongPtrArray *PdrgpulScanIds(IMemoryPool *mp) const;

			// add part filter to map
			void AddPartFilter
				(
				IMemoryPool *mp,
				ULONG scan_id,
				CExpression *pexpr,
				IStatistics *stats
				);

			// look for given scan id in given map and, if found, copy the corresponding entry to current map
			BOOL FCopyPartFilter(IMemoryPool *mp, ULONG scan_id, CPartFilterMap *ppfmSource);

			// copy all part filters from source map to current map
			void CopyPartFilterMap(IMemoryPool *mp, CPartFilterMap *ppfmSource);

			// print function
			IOstream &OsPrint(IOstream &os) const;

#ifdef GPOS_DEBUG
			// debug print for interactive debugging sessions only
			void DbgPrint() const;
#endif // GPOS_DEBUG

	}; // class CPartFilterMap

}

#endif // !GPOPT_CPartFilterMap_H

// EOF

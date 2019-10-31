//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalNAryJoin.h
//
//	@doc:
//		N-ary inner join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalNAryJoin_H
#define GPOS_CLogicalNAryJoin_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{	
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalNAryJoin
	//
	//	@doc:
	//		N-ary inner join operator
	//
	//---------------------------------------------------------------------------
	class CLogicalNAryJoin : public CLogicalJoin
	{
		private:

			// private copy ctor
			CLogicalNAryJoin(const CLogicalNAryJoin &);

			ULongPtrArray *m_lojChildIndexes;

		public:

			// ctor
			explicit
			CLogicalNAryJoin(CMemoryPool *mp);

			CLogicalNAryJoin(CMemoryPool *mp, ULongPtrArray *lojChildIndexes);

			// dtor
			virtual
			~CLogicalNAryJoin() 
			{ CRefCount::SafeRelease(m_lojChildIndexes); }

			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopLogicalNAryJoin;
			}
			
			// return a string for operator name
			virtual 
			const CHAR *SzId() const
			{
				return "CLogicalNAryJoin";
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive not nullable columns
			virtual
			CColRefSet *DeriveNotNullColumns
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PcrsDeriveNotNullCombineLogical(mp, exprhdl);
			}

			// derive partition consumer info
			virtual
			CPartInfo *DerivePartitionInfo
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl
				) 
				const
			{
				return PpartinfoDeriveCombine(mp, exprhdl);
			}

			// derive max card
			virtual
			CMaxCard DeriveMaxCard(CMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive constraint property
			virtual
			CPropConstraint *DerivePropertyConstraint
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpcDeriveConstraintFromPredicates(mp, exprhdl);
			}

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

			// promise level for stat derivation
			virtual
			EStatPromise Esp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				// we should use the expanded join order for stat derivation
				return EspLow;
			}

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(CMemoryPool *mp) const;

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CLogicalNAryJoin *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);

				return dynamic_cast<CLogicalNAryJoin*>(pop);
			}

			BOOL
			HasOuterJoinChildren() const
			{
				return (NULL != m_lojChildIndexes);
			}

			BOOL
			IsInnerJoinChild(ULONG child_num) const
			{
				return (NULL == m_lojChildIndexes || *((*m_lojChildIndexes)[child_num]) == 0);
			}

			virtual
			IOstream & OsPrint(IOstream &os) const;

	}; // class CLogicalNAryJoin

}


#endif // !GPOS_CLogicalNAryJoin_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftSemiJoin.h
//
//	@doc:
//		Left semi join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalLeftSemiJoin_H
#define GPOS_CLogicalLeftSemiJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalLeftSemiJoin
	//
	//	@doc:
	//		Left semi join operator
	//
	//---------------------------------------------------------------------------
	class CLogicalLeftSemiJoin : public CLogicalJoin
	{
		private:

			// private copy ctor
			CLogicalLeftSemiJoin(const CLogicalLeftSemiJoin &);

		public:

			// ctor
			explicit
			CLogicalLeftSemiJoin(IMemoryPool *mp);

			// dtor
			virtual
			~CLogicalLeftSemiJoin()
			{}

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalLeftSemiJoin;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalLeftSemiJoin";
			}

			// return true if we can pull projections up past this operator from its given child
			virtual
			BOOL FCanPullProjectionsUp
				(
				ULONG child_index
				) const
			{
				return (0 == child_index);
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput(IMemoryPool *mp, CExpressionHandle &hdl);
			
			// derive not nullable output columns
			virtual
			CColRefSet *PcrsDeriveNotNull
				(
				IMemoryPool *,// mp
				CExpressionHandle &exprhdl
				)
				const
			{
				return PcrsDeriveNotNullPassThruOuter(exprhdl);
			}

			// dervive keys
			virtual 
			CKeyCollection *PkcDeriveKeys(IMemoryPool *mp, CExpressionHandle &exprhdl) const;
			
			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint
				(
				IMemoryPool *, //mp,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
			}

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			CXformSet *PxfsCandidates(IMemoryPool *mp) const;

			// derive statistics
			virtual
			IStatistics *PstatsDerive
						(
						IMemoryPool *mp,
						CExpressionHandle &exprhdl,
						IStatisticsArray *stats_ctxt
						)
						const;

			// promise level for stat derivation
			virtual
			EStatPromise Esp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				// semi join can be converted to inner join, which is used for stat derivation
				return EspMedium;
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CLogicalLeftSemiJoin *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalLeftSemiJoin == pop->Eopid());

				return dynamic_cast<CLogicalLeftSemiJoin*>(pop);
			}

			// derive statistics
			static
			IStatistics *PstatsDerive
				(
				IMemoryPool *mp,
				CStatsPredJoinArray *join_preds_stats,
				IStatistics *outer_stats,
				IStatistics *inner_side_stats
				);

	}; // class CLogicalLeftSemiJoin

}


#endif // !GPOS_CLogicalLeftSemiJoin_H

// EOF

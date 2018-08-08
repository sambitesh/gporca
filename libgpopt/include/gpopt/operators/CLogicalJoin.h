//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalJoin.h
//
//	@doc:
//		Base class of all logical join operators
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalJoin_H
#define GPOS_CLogicalJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalJoin
	//
	//	@doc:
	//		join operator
	//
	//---------------------------------------------------------------------------
	class CLogicalJoin : public CLogical
	{
		private:

			// private copy ctor
			CLogicalJoin(const CLogicalJoin &);

		protected:

			// ctor
			explicit
			CLogicalJoin(IMemoryPool *mp);
		
			// dtor
			virtual 
			~CLogicalJoin() 
			{}

		public:
		
			// match function
			virtual
			BOOL Matches(COperator *pop) const;


			// sensitivity to order of inputs
			BOOL FInputOrderSensitive() const
			{
				return true;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns
						(
						IMemoryPool *, //mp,
						UlongToColRefMap *, //colref_mapping,
						BOOL //must_exist
						)
			{
				return PopCopyDefault();
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl
				)
			{
				return PcrsDeriveOutputCombineLogical(mp, exprhdl);
			}
					
			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl
				) 
				const
			{
				return PpartinfoDeriveCombine(mp, exprhdl);
			}

			
			// derive keys
			CKeyCollection *PkcDeriveKeys
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PkcCombineKeys(mp, exprhdl);
			}

			// derive function properties
			virtual
			CFunctionProp *PfpDerive
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PfpDeriveFromScalar(mp, exprhdl, exprhdl.Arity() - 1);
			}

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

			// promise level for stat derivation
			virtual
			EStatPromise Esp
				(
				CExpressionHandle &exprhdl
				)
				const
			{
				// no stat derivation on Join trees with subqueries
				if (exprhdl.GetDrvdScalarProps(exprhdl.Arity() - 1)->FHasSubquery())
				{
					 return EspLow;
				}

				if (NULL != exprhdl.Pgexpr() &&
					exprhdl.Pgexpr()->ExfidOrigin() == CXform::ExfExpandNAryJoin)
				{
					return EspMedium;
				}

				return EspHigh;
			}

			// derive statistics
			virtual
			IStatistics *PstatsDerive
						(
						IMemoryPool *mp,
						CExpressionHandle &exprhdl,
						IStatisticsArray *stats_ctxt
						)
						const;

			//-------------------------------------------------------------------------------------
			// Required Relational Properties
			//-------------------------------------------------------------------------------------

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat
					(
					IMemoryPool *mp,
					CExpressionHandle &exprhdl,
					CColRefSet *pcrsInput,
					ULONG child_index
					)
					const
			{
				const ULONG arity = exprhdl.Arity();

				return PcrsReqdChildStats(mp, exprhdl, pcrsInput, exprhdl.GetDrvdScalarProps(arity - 1)->PcrsUsed(), child_index);
			}

			// return true if operator can select a subset of input tuples based on some predicate
			virtual
			BOOL FSelectionOp() const
			{
				return true;
			}

	}; // class CLogicalJoin

}


#endif // !GPOS_CLogicalJoin_H

// EOF

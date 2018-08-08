//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalAgg.h
//
//	@doc:
//		Basic physical aggregate operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalAgg_H
#define GPOS_CPhysicalAgg_H

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPhysical.h"


namespace gpopt
{
	// fwd declaration
	class CDistributionSpec;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalAgg
	//
	//	@doc:
	//		Aggregate operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalAgg : public CPhysical
	{
		private:

			// private copy ctor
			CPhysicalAgg(const CPhysicalAgg &);
			
			// array of grouping columns
			CColRefArray *m_pdrgpcr;

			// aggregate type (local / intermediate / global)
			COperator::EGbAggType m_egbaggtype;

			// compute required distribution of the n-th child of an intermediate aggregate
			CDistributionSpec *PdsRequiredIntermediateAgg(IMemoryPool *mp, ULONG  ulOptReq) const;

			// compute required distribution of the n-th child of a global aggregate
			CDistributionSpec *PdsRequiredGlobalAgg
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG child_index,
				CColRefArray *pdrgpcrGrp,
				CColRefArray *pdrgpcrGrpMinimal,
				ULONG  ulOptReq
				)
				const;

			// compute a maximal hashed distribution using the given columns,
			// if no such distribution can be created, return a Singleton distribution
			static
			CDistributionSpec *PdsMaximalHashed(IMemoryPool *mp, CColRefArray *colref_array);

		protected:

			// array of minimal grouping columns based on FDs
			CColRefArray *m_pdrgpcrMinimal;

			// could the local / intermediate / global aggregate generate
			// duplicate values for the same group across segments
			BOOL m_fGeneratesDuplicates;

			// array of columns used in distinct qualified aggregates (DQA)
			// used only in the case of intermediate aggregates
			CColRefArray *m_pdrgpcrArgDQA;

			// is agg part of multi-stage aggregation
			BOOL m_fMultiStage;

			// compute required columns of the n-th child
			CColRefSet *PcrsRequiredAgg
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsRequired,
				ULONG child_index,
				CColRefArray *pdrgpcrGrp
				);

			// compute required distribution of the n-th child
			CDistributionSpec *PdsRequiredAgg
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG child_index,
				ULONG  ulOptReq,
				CColRefArray *pdrgpcgGrp,
				CColRefArray *pdrgpcrGrpMinimal
				)
				const;

		public:

			// ctor
			CPhysicalAgg
				(
				IMemoryPool *mp,
				CColRefArray *colref_array,
				CColRefArray *pdrgpcrMinimal, // FD's on grouping columns
				COperator::EGbAggType egbaggtype,
				BOOL fGeneratesDuplicates,
				CColRefArray *pdrgpcrArgDQA,
				BOOL fMultiStage
				);

			// dtor
			virtual
			~CPhysicalAgg();

			// does this aggregate generate duplicate values for the same group
			virtual
			BOOL FGeneratesDuplicates() const
			{
				return m_fGeneratesDuplicates;
			}

			virtual
			const CColRefArray *PdrgpcrGroupingCols() const
			{
				return m_pdrgpcr;
			}

			// array of columns used in distinct qualified aggregates (DQA)
			virtual
			const CColRefArray *PdrgpcrArgDQA() const
			{
				return m_pdrgpcrArgDQA;
			}

			// aggregate type
			COperator::EGbAggType Egbaggtype() const
			{
				return m_egbaggtype;
			}

			// is a global aggregate?
			BOOL FGlobal() const
			{
				return (COperator::EgbaggtypeGlobal == m_egbaggtype);
			}

			// is agg part of multi-stage aggregation
			BOOL FMultiStage() const
			{
				return m_fMultiStage;
			}

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// hash function
			virtual
			ULONG HashValue() const;

			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const
			{
				return true;
			}
		
			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required output columns of the n-th child
			virtual
			CColRefSet *PcrsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				);

			// compute required ctes of the n-th child
			virtual
			CCTEReq *PcteRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CCTEReq *pcter,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// compute required distribution of the n-th child
			virtual
			CDistributionSpec *PdsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired,
				ULONG child_index,
				CDrvdPropArrays *, //pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const
			{
				return PdsRequiredAgg(mp, exprhdl, pdsRequired, child_index, ulOptReq, m_pdrgpcr, m_pdrgpcrMinimal);
			}

			// compute required rewindability of the n-th child
			virtual
			CRewindabilitySpec *PrsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// check if required columns are included in output columns
			virtual
			BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired, ULONG ulOptReq) const;

			
			// compute required partition propagation of the n-th child
			virtual
			CPartitionPropagationSpec *PppsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CPartitionPropagationSpec *pppsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				);
			
			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive distribution
			virtual
			CDistributionSpec *PdsDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive rewindability
			virtual
			CRewindabilitySpec *PrsDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive partition index map
			virtual
			CPartIndexMap *PpimDerive
				(
				IMemoryPool *, // mp
				CExpressionHandle &exprhdl,
				CDrvdPropCtxt * //pdpctxt
				)
				const
			{
				return PpimPassThruOuter(exprhdl);
			}
			
			// derive partition filter map
			virtual
			CPartFilterMap *PpfmDerive
				(
				IMemoryPool *, // mp
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpfmPassThruOuter(exprhdl);
			}

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------

			// return distribution property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetDistribution
				(
				CExpressionHandle &exprhdl,
				const CEnfdDistribution *ped
				) 
				const;

			// return rewindability property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetRewindability
				(
				CExpressionHandle &, // exprhdl
				const CEnfdRewindability * // per
				)
				const;

			// return true if operator passes through stats obtained from children,
			// this is used when computing stats during costing
			virtual
			BOOL FPassThruStats() const
			{
				return false;
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CPhysicalAgg *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(CUtils::FPhysicalAgg(pop));

				return dynamic_cast<CPhysicalAgg*>(pop);
			}

			// debug print
			virtual 
			IOstream &OsPrint(IOstream &os) const;

	}; // class CPhysicalAgg

}


#endif // !GPOS_CPhysicalAgg_H

// EOF

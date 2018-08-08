//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalFilter.cpp
//
//	@doc:
//		Implementation of filter operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/base/CDistributionSpecAny.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalFilter.h"
#include "gpopt/operators/CPredicateUtils.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::CPhysicalFilter
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalFilter::CPhysicalFilter
	(
	IMemoryPool *mp
	)
	:
	CPhysical(mp)
{
	// when Filter includes outer references, correlated execution has to be enforced,
	// in this case, we create two child optimization requests to guarantee correct evaluation of parameters
	// (1) Broadcast
	// (2) Singleton

	SetDistrRequests(2 /*ulDistrReqs*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::~CPhysicalFilter
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalFilter::~CPhysicalFilter()
{}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PcrsRequired
//
//	@doc:
//		Compute required output columns of the n-th child
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalFilter::PcrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG child_index,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index && "Required properties can only be computed on the relational child");

	return PcrsChildReqd(mp, exprhdl, pcrsRequired, child_index, 1 /*ulScalarIndex*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalFilter::PosRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	COrderSpec *posRequired,
	ULONG child_index,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	return PosPassThru(mp, exprhdl, posRequired, child_index);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalFilter::PdsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CDistributionSpec *pdsRequired,
	ULONG child_index,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG ulOptReq
	)
	const
{
	if (CDistributionSpec::EdtAny == pdsRequired->Edt() &&
		CDistributionSpecAny::PdsConvert(pdsRequired)->FAllowOuterRefs())
	{
		// this situation arises when we have Filter on top of (Dynamic)IndexScan,
		// in this case, we impose no distribution requirements even with the presence of outer references,
		// the reason is that the Filter must be the inner child of IndexNLJoin and
		// we need to have outer references referring to join's outer child
		pdsRequired->AddRef();
		return pdsRequired;
	}

	return CPhysical::PdsUnary(mp, exprhdl, pdsRequired, child_index, ulOptReq);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalFilter::PrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CRewindabilitySpec *prsRequired,
	ULONG child_index,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	// If there are outer references in the Filter (but none coming from the
	// child), we can optimize by adding a materialize in between. However, if
	// there are outer references in the child, we should *not* add a materialize
	// here.  Otherwise the child will not get rescanned leading to wrong
	// results.
	if (exprhdl.HasOuterRefs() && !exprhdl.HasOuterRefs(0))
	{
		return GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtGeneral);
	}

	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PppsRequired
//
//	@doc:
//		Compute required partition propagation of the n-th child
//
//---------------------------------------------------------------------------
CPartitionPropagationSpec *
CPhysicalFilter::PppsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CPartitionPropagationSpec *pppsRequired,
	ULONG 
#ifdef GPOS_DEBUG
	child_index
#endif
	,
	CDrvdPropArrays *, //pdrgpdpCtxt,
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);
	GPOS_ASSERT(NULL != pppsRequired);
	
	CPartIndexMap *ppimReqd = pppsRequired->Ppim();
	CPartFilterMap *ppfmReqd = pppsRequired->Ppfm();
	
	ULongPtrArray *pdrgpul = ppimReqd->PdrgpulScanIds(mp);
	
	CPartIndexMap *ppimResult = GPOS_NEW(mp) CPartIndexMap(mp);
	CPartFilterMap *ppfmResult = GPOS_NEW(mp) CPartFilterMap(mp);
	
	/// get derived part consumers
	CPartInfo *ppartinfo = exprhdl.GetRelationalProperties(0)->Ppartinfo();
	
	const ULONG ulPartIndexIds = pdrgpul->Size();
	BOOL fUseConstraints = (1 == exprhdl.GetRelationalProperties()->JoinDepth());
	
	for (ULONG ul = 0; ul < ulPartIndexIds; ul++)
	{
		ULONG part_idx_id = *((*pdrgpul)[ul]);

		if (!ppartinfo->FContainsScanId(part_idx_id))
		{
			// part index id does not exist in child nodes: no need to push through
			// the request
			continue;
		}

		ppimResult->AddRequiredPartPropagation(ppimReqd, part_idx_id, CPartIndexMap::EppraPreservePropagators);
		
		// look for a filter on the part key
		CExpression *pexprScalar = exprhdl.PexprScalarChild(1 /*child_index*/);

		CExpression *pexprCmp = NULL;
		CPartKeysArray *pdrgppartkeys = ppimReqd->Pdrgppartkeys(part_idx_id);
		const ULONG ulKeysets = pdrgppartkeys->Size();
		for (ULONG ulKey = 0; NULL == pexprCmp && ulKey < ulKeysets; ulKey++)
		{
			// get partition key
			CColRefArrays *pdrgpdrgpcrPartKeys = (*pdrgppartkeys)[ulKey]->Pdrgpdrgpcr();

			// try to generate a request with dynamic partition selection		
			pexprCmp = CPredicateUtils::PexprExtractPredicatesOnPartKeys
									(
									mp,
									pexprScalar,
									pdrgpdrgpcrPartKeys,
									NULL, /*pcrsAllowedRefs*/
									fUseConstraints
									);
		}
				
		if (NULL == pexprCmp)
		{
			// no comparison found in filter: check if a comparison was already
			// specified in the required partition propagation
			if (ppfmReqd->FContainsScanId(part_idx_id))
			{
				pexprCmp = ppfmReqd->Pexpr(part_idx_id);
				pexprCmp->AddRef();
			}
			
			// TODO:  - May 31, 2012; collect multiple comparisons on the 
			// partition keys
		}
		
		if (NULL != pexprCmp)
		{
			// interesting filter found
			ppfmResult->AddPartFilter(mp, part_idx_id, pexprCmp, NULL /*stats */);
		}
	}
	
	pdrgpul->Release();

	return GPOS_NEW(mp) CPartitionPropagationSpec(ppimResult, ppfmResult);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalFilter::PcteRequired
	(
	IMemoryPool *, //mp,
	CExpressionHandle &, //exprhdl,
	CCTEReq *pcter,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif
	,
	CDrvdPropArrays *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);
	return PcterPushThru(pcter);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalFilter::PosDerive
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PosDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalFilter::PdsDerive
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PdsDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalFilter::PrsDerive
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PrsDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalFilter::Matches
	(
	COperator *pop
	)
	const
{
	// filter doesn't contain any members as of now
	return Eopid() == pop->Eopid();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalFilter::FProvidesReqdCols
	(
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG // ulOptReq
	)
	const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalFilter::EpetOrder
	(
	CExpressionHandle &, // exprhdl
	const CEnfdOrder *
#ifdef GPOS_DEBUG
	peo
#endif // GPOS_DEBUG
	)
	const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	// always force sort to be on top of filter
	return CEnfdProp::EpetRequired;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFilter::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalFilter::EpetRewindability
	(
	CExpressionHandle &exprhdl,
	const CEnfdRewindability *per
	)
	const
{
	// get rewindability delivered by the Filter node
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		 // required rewindability is already provided
		 return CEnfdProp::EpetUnnecessary;
	}

	// always force spool to be on top of filter
	return CEnfdProp::EpetRequired;
}


// EOF


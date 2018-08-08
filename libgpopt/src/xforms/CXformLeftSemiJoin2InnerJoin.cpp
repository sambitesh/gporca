//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftSemiJoin2InnerJoin.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformLeftSemiJoin2InnerJoin.h"



using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformLeftSemiJoin2InnerJoin::CXformLeftSemiJoin2InnerJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformLeftSemiJoin2InnerJoin::CXformLeftSemiJoin2InnerJoin
	(
	IMemoryPool *mp
	)
	:
	// pattern
	CXformExploration
		(
		GPOS_NEW(mp) CExpression
					(
					mp,
					GPOS_NEW(mp) CLogicalLeftSemiJoin(mp),
					GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)), // left child
					GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)), // right child
					GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))  // predicate
					)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformLeftSemiJoin2InnerJoin::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLeftSemiJoin2InnerJoin::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	if (exprhdl.HasOuterRefs() || exprhdl.GetDrvdScalarProps(2)->FHasSubquery())
	{
		return ExfpNone;
	}

	CColRefSet *pcrsInnerOutput = exprhdl.GetRelationalProperties(1 /*child_index*/)->PcrsOutput();
	CExpression *pexprScalar = exprhdl.PexprScalarChild(2 /*child_index*/);
	CAutoMemoryPool amp;

	// examine join predicate to determine xform applicability
	if (!CPredicateUtils::FSimpleEqualityUsingCols(amp.Pmp(), pexprScalar, pcrsInnerOutput))
	{
		return ExfpNone;
	}

	return ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformLeftSemiJoin2InnerJoin::Transform
//
//	@doc:
//		actual transformation
//
//---------------------------------------------------------------------------
void
CXformLeftSemiJoin2InnerJoin::Transform
	(
	CXformContext *pxfctxt,
	CXformResult *pxfres,
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	IMemoryPool *mp = pxfctxt->Pmp();

	// extract components
	CExpression *pexprOuter = (*pexpr)[0];
	CExpression *pexprInner = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	pexprOuter->AddRef();
	pexprInner->AddRef();
	pexprScalar->AddRef();

	// construct grouping columns by collecting used columns in the join predicate
	// that come from join's inner child
	CColRefSet *pcrsOuterOutput = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsUsed = CDrvdPropScalar::GetDrvdScalarProps(pexprScalar->PdpDerive())->PcrsUsed();
	CColRefSet *pcrsGb = GPOS_NEW(mp) CColRefSet(mp);
	pcrsGb->Include(pcrsUsed);
	pcrsGb->Difference(pcrsOuterOutput);
	GPOS_ASSERT(0 < pcrsGb->Size());

	CKeyCollection *pkc = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->Pkc();
	if (NULL == pkc ||
		(NULL != pkc && !pkc->FKey(pcrsGb, false /*fExactMatch*/)))
	{
		// grouping columns do not cover a key on the inner side,
		// we need to create a group by on inner side
		CColRefArray *colref_array = pcrsGb->Pdrgpcr(mp);
		CExpression *pexprGb =
			GPOS_NEW(mp) CExpression
				(
				mp,
				GPOS_NEW(mp) CLogicalGbAgg(mp, colref_array, COperator::EgbaggtypeGlobal /*egbaggtype*/),
				pexprInner,
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp))
				);
		pexprInner = pexprGb;
	}

	CExpression *pexprInnerJoin =
		CUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp, pexprOuter, pexprInner, pexprScalar);

	pcrsGb->Release();
	pxfres->Add(pexprInnerJoin);
}


// EOF

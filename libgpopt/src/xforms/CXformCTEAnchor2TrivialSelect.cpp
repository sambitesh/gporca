//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformCTEAnchor2TrivialSelect.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformCTEAnchor2TrivialSelect.h"
#include "gpopt/xforms/CXformUtils.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformCTEAnchor2TrivialSelect::CXformCTEAnchor2TrivialSelect
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformCTEAnchor2TrivialSelect::CXformCTEAnchor2TrivialSelect
	(
	IMemoryPool *mp
	)
	:
	CXformExploration
		(
		 // pattern
		GPOS_NEW(mp) CExpression
				(
				mp,
				GPOS_NEW(mp) CLogicalCTEAnchor(mp),
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformCTEAnchor2TrivialSelect::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformCTEAnchor2TrivialSelect::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	ULONG id = CLogicalCTEAnchor::PopConvert(exprhdl.Pop())->Id();
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	const ULONG ulConsumers = pcteinfo->UlConsumers(id);
	GPOS_ASSERT(0 < ulConsumers);

	if ((pcteinfo->FEnableInlining() || 1 == ulConsumers) &&
		CXformUtils::FInlinableCTE(id))
	{
		return CXform::ExfpHigh;
	}

	return CXform::ExfpNone;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformCTEAnchor2TrivialSelect::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformCTEAnchor2TrivialSelect::Transform
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

	// child of CTE anchor
	CExpression *pexprChild = (*pexpr)[0];
	pexprChild->AddRef();

	CExpression *pexprSelect =
		GPOS_NEW(mp) CExpression
			(
			mp,
			GPOS_NEW(mp) CLogicalSelect(mp),
			pexprChild,
			CUtils::PexprScalarConstBool(mp, true /*fValue*/)
			);

	pxfres->Add(pexprSelect);
}

// EOF

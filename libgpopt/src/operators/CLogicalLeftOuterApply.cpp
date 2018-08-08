//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterApply.cpp
//
//	@doc:
//		Implementation of left outer apply operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftOuterApply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterApply::CLogicalLeftOuterApply
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalLeftOuterApply::CLogicalLeftOuterApply
	(
	IMemoryPool *mp
	)
	:
	CLogicalApply(mp)
{
	GPOS_ASSERT(NULL != mp);

	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterApply::CLogicalLeftOuterApply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalLeftOuterApply::CLogicalLeftOuterApply
	(
	IMemoryPool *mp,
	CColRefArray *pdrgpcrInner,
	EOperatorId eopidOriginSubq
	)
	:
	CLogicalApply(mp, pdrgpcrInner, eopidOriginSubq)
{
	GPOS_ASSERT(0 < pdrgpcrInner->Size());
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterApply::~CLogicalLeftOuterApply
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalLeftOuterApply::~CLogicalLeftOuterApply()
{}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterApply::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalLeftOuterApply::Maxcard
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, exprhdl.GetRelationalProperties(0)->Maxcard());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterApply::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftOuterApply::PxfsCandidates
	(
	IMemoryPool *mp
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfLeftOuterApply2LeftOuterJoin);
	(void) xform_set->ExchangeSet(CXform::ExfLeftOuterApply2LeftOuterJoinNoCorrelations);

	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterApply::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalLeftOuterApply::PopCopyWithRemappedColumns
	(
	IMemoryPool *mp,
	UlongToColRefMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRefArray *pdrgpcrInner = CUtils::PdrgpcrRemap(mp, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalLeftOuterApply(mp, pdrgpcrInner, m_eopidOriginSubq);
}

// EOF


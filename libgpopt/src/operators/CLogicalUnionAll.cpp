//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUnionAll.cpp
//
//	@doc:
//		Implementation of UnionAll operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CLogicalUnionAll.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "naucrates/statistics/CUnionAllStatsProcessor.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::CLogicalUnionAll
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalUnionAll::CLogicalUnionAll
	(
	IMemoryPool *mp
	)
	:
	CLogicalUnion(mp),
	m_ulScanIdPartialIndex(0)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::CLogicalUnionAll
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalUnionAll::CLogicalUnionAll
	(
	IMemoryPool *mp,
	CColRefArray *pdrgpcrOutput,
	CColRefArrays *pdrgpdrgpcrInput,
	ULONG ulScanIdPartialIndex
	)
	:
	CLogicalUnion(mp, pdrgpcrOutput, pdrgpdrgpcrInput),
	m_ulScanIdPartialIndex(ulScanIdPartialIndex)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::~CLogicalUnionAll
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalUnionAll::~CLogicalUnionAll()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalUnionAll::Maxcard
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	const ULONG arity = exprhdl.Arity();

	CMaxCard maxcard = exprhdl.GetRelationalProperties(0)->Maxcard();
	for (ULONG ul = 1; ul < arity; ul++)
	{
		maxcard += exprhdl.GetRelationalProperties(ul)->Maxcard();
	}

	return maxcard;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalUnionAll::PopCopyWithRemappedColumns
	(
	IMemoryPool *mp,
	UlongToColRefMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRefArray *pdrgpcrOutput = CUtils::PdrgpcrRemap(mp, m_pdrgpcrOutput, colref_mapping, must_exist);
	CColRefArrays *pdrgpdrgpcrInput = CUtils::PdrgpdrgpcrRemap(mp, m_pdrgpdrgpcrInput, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalUnionAll(mp, pdrgpcrOutput, pdrgpdrgpcrInput, m_ulScanIdPartialIndex);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalUnionAll::PkcDeriveKeys
	(
	IMemoryPool *, //mp,
	CExpressionHandle & // exprhdl
	)
	const
{
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalUnionAll::PxfsCandidates
	(
	IMemoryPool *mp
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfImplementUnionAll);

	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::PstatsDeriveUnionAll
//
//	@doc:
//		Derive statistics based on union all semantics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalUnionAll::PstatsDeriveUnionAll
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(COperator::EopLogicalUnionAll == exprhdl.Pop()->Eopid() || COperator::EopLogicalUnion == exprhdl.Pop()->Eopid());

	CColRefArray *pdrgpcrOutput = CLogicalSetOp::PopConvert(exprhdl.Pop())->PdrgpcrOutput();
	CColRefArrays *pdrgpdrgpcrInput = CLogicalSetOp::PopConvert(exprhdl.Pop())->PdrgpdrgpcrInput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	GPOS_ASSERT(NULL != pdrgpdrgpcrInput);

	IStatistics *result_stats = exprhdl.Pstats(0);
	result_stats->AddRef();
	const ULONG arity = exprhdl.Arity();
	for (ULONG ul = 1; ul < arity; ul++)
	{
		IStatistics *child_stats = exprhdl.Pstats(ul);
		CStatistics *stats = CUnionAllStatsProcessor::CreateStatsForUnionAll
											(
											mp,
											dynamic_cast<CStatistics *>(result_stats),
											dynamic_cast<CStatistics *>(child_stats),
											CColRef::Pdrgpul(mp, pdrgpcrOutput),
											CColRef::Pdrgpul(mp, (*pdrgpdrgpcrInput)[0]),
											CColRef::Pdrgpul(mp, (*pdrgpdrgpcrInput)[ul])
											);
		result_stats->Release();
		result_stats = stats;
	}

	return result_stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnionAll::PstatsDerive
//
//	@doc:
//		Derive statistics based on union all semantics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalUnionAll::PstatsDerive
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	IStatisticsArray * // not used
	)
	const
{
	GPOS_ASSERT(EspNone < Esp(exprhdl));

	return PstatsDeriveUnionAll(mp, exprhdl);
}

// EOF

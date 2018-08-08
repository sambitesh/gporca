//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDifferenceAll.cpp
//
//	@doc:
//		Implementation of DifferenceAll operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/CLogicalDifferenceAll.h"
#include "gpopt/operators/CExpressionHandle.h"

#include "naucrates/statistics/CStatsPredUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::CLogicalDifferenceAll
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDifferenceAll::CLogicalDifferenceAll
	(
	IMemoryPool *mp
	)
	:
	CLogicalSetOp(mp)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::CLogicalDifferenceAll
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalDifferenceAll::CLogicalDifferenceAll
	(
	IMemoryPool *mp,
	CColRefArray *pdrgpcrOutput,
	CColRefArrays *pdrgpdrgpcrInput
	)
	:
	CLogicalSetOp(mp, pdrgpcrOutput, pdrgpdrgpcrInput)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::~CLogicalDifferenceAll
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalDifferenceAll::~CLogicalDifferenceAll()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalDifferenceAll::Maxcard
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	// contradictions produce no rows
	if (CDrvdPropRelational::GetRelationalProperties(exprhdl.Pdp())->Ppc()->FContradiction())
	{
		return CMaxCard(0 /*ull*/);
	}

	return exprhdl.GetRelationalProperties(0)->Maxcard();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDifferenceAll::PopCopyWithRemappedColumns
	(
	IMemoryPool *mp,
	UlongToColRefMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRefArray *pdrgpcrOutput = CUtils::PdrgpcrRemap(mp, m_pdrgpcrOutput, colref_mapping, must_exist);
	CColRefArrays *pdrgpdrgpcrInput = CUtils::PdrgpdrgpcrRemap(mp, m_pdrgpdrgpcrInput, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalDifferenceAll(mp, pdrgpcrOutput, pdrgpdrgpcrInput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalDifferenceAll::PkcDeriveKeys
	(
	IMemoryPool *, // mp,
	CExpressionHandle & //exprhdl
	)
	const
{
	// TODO: Add keys on columns contributing to the setop from the outer child
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalDifferenceAll::PstatsDerive
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	IStatisticsArray * // not used
	)
	const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);

	// difference all is transformed into a LASJ,
	// we follow the same route to compute statistics
	CColRefSetArray *output_colrefsets = GPOS_NEW(mp) CColRefSetArray(mp);
	const ULONG size = m_pdrgpdrgpcrInput->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, (*m_pdrgpdrgpcrInput)[ul]);
		output_colrefsets->Append(pcrs);
	}

	IStatistics *outer_stats = exprhdl.Pstats(0);
	IStatistics *inner_side_stats = exprhdl.Pstats(1);

	// construct the scalar condition for the LASJ
	CExpression *pexprScCond = CUtils::PexprConjINDFCond(mp, m_pdrgpdrgpcrInput);

	// compute the statistics for LASJ
	CColRefSet *outer_refs = exprhdl.GetRelationalProperties()->PcrsOuter();
	CStatsPredJoinArray *join_preds_stats = CStatsPredUtils::ExtractJoinStatsFromExpr
														(
														mp, 
														exprhdl, 
														pexprScCond, 
														output_colrefsets, 
														outer_refs
														);
	IStatistics *LASJ_stats = outer_stats->CalcLASJoinStats
											(
											mp,
											inner_side_stats,
											join_preds_stats,
											true /* DoIgnoreLASJHistComputation*/
											);

	// clean up
	pexprScCond->Release();
	join_preds_stats->Release();
	output_colrefsets->Release();

	return LASJ_stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDifferenceAll::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDifferenceAll::PxfsCandidates
	(
	IMemoryPool *mp
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfDifferenceAll2LeftAntiSemiJoin);
	return xform_set;
}

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftAntiSemiJoin.cpp
//
//	@doc:
//		Implementation of left anti semi join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "naucrates/statistics/CStatsPredUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiJoin::CLogicalLeftAntiSemiJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalLeftAntiSemiJoin::CLogicalLeftAntiSemiJoin
	(
	IMemoryPool *mp
	)
	:
	CLogicalJoin(mp)
{
	GPOS_ASSERT(NULL != mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiJoin::MaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalLeftAntiSemiJoin::MaxCard
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	// pass on max card of first child
	return exprhdl.GetRelationalProperties(0)->Maxcard();
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiJoin::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftAntiSemiJoin::PxfsCandidates
	(
	IMemoryPool *mp
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfAntiSemiJoinAntiSemiJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfAntiSemiJoinAntiSemiJoinNotInSwap);
	(void) xform_set->ExchangeSet(CXform::ExfAntiSemiJoinSemiJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfAntiSemiJoinInnerJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfLeftAntiSemiJoin2CrossProduct);
	(void) xform_set->ExchangeSet(CXform::ExfLeftAntiSemiJoin2NLJoin);
	(void) xform_set->ExchangeSet(CXform::ExfLeftAntiSemiJoin2HashJoin);
	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiJoin::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalLeftAntiSemiJoin::PcrsDeriveOutput
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(3 == exprhdl.Arity());

	return PcrsDeriveOutputPassThru(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiJoin::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalLeftAntiSemiJoin::PkcDeriveKeys
	(
	IMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftAntiSemiJoin::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalLeftAntiSemiJoin::PstatsDerive
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	IStatisticsArray * // not used
	)
	const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);
	IStatistics *outer_stats = exprhdl.Pstats(0);
	IStatistics *inner_side_stats = exprhdl.Pstats(1);
	CStatsPredJoinArray *join_preds_stats = CStatsPredUtils::ExtractJoinStatsFromExprHandle(mp, exprhdl);
	IStatistics *pstatsLASJoin = outer_stats->CalcLASJoinStats
												(
												mp,
												inner_side_stats,
												join_preds_stats,
												true /* DoIgnoreLASJHistComputation */
												);

	// clean up
	join_preds_stats->Release();

	return pstatsLASJoin;
}
// EOF


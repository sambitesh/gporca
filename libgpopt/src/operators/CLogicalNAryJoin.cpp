//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalNAryJoin.cpp
//
//	@doc:
//		Implementation of n-ary inner join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColumnFactory.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CLogicalNAryJoin.h"
#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalNAryJoin::CLogicalNAryJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalNAryJoin::CLogicalNAryJoin
	(
	CMemoryPool *mp
	)
	:
	CLogicalJoin(mp),
	m_lojChildIndexes(NULL)
{
	GPOS_ASSERT(NULL != mp);
}

CLogicalNAryJoin::CLogicalNAryJoin
	(
	CMemoryPool *mp,
	ULongPtrArray *lojChildIndexes
	)
	:
	CLogicalJoin(mp),
	m_lojChildIndexes(lojChildIndexes)
{
	GPOS_ASSERT(NULL != mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalNAryJoin::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalNAryJoin::DeriveMaxCard
	(
	CMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return CLogical::Maxcard(exprhdl, exprhdl.Arity() - 1, MaxcardDef(exprhdl));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalNAryJoin::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalNAryJoin::PxfsCandidates
	(
	CMemoryPool *mp
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	
	(void) xform_set->ExchangeSet(CXform::ExfSubqNAryJoin2Apply);
	(void) xform_set->ExchangeSet(CXform::ExfExpandNAryJoin);
	(void) xform_set->ExchangeSet(CXform::ExfExpandNAryJoinMinCard);
	(void) xform_set->ExchangeSet(CXform::ExfExpandNAryJoinDP);
	(void) xform_set->ExchangeSet(CXform::ExfExpandNAryJoinGreedy);
	(void) xform_set->ExchangeSet(CXform::ExfExpandNAryJoinDPv2);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalNAryJoin::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalNAryJoin::OsPrint
(
 IOstream &os
 )
const
{
	os	<< SzId();

	if (NULL != m_lojChildIndexes)
	{
		// print out the indexes of the logical children that correspond to
		// the scalar child entries below the CScalarNAryJoinPredList
		os << " [";
		ULONG size = m_lojChildIndexes->Size();
		for (ULONG ul=0; ul < size; ul++)
		{
			if (0 < ul)
			{
				os << ", ";
			}
			os << *((*m_lojChildIndexes)[ul]);
		}
		os	<< "]";
	}

	return os;
}



// EOF


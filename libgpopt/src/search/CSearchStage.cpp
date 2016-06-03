//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSearchStage.cpp
//
//	@doc:
//		Implementation of optimizer search stage
//
//	@owner:
//		
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/search/CSearchStage.h"
#include "gpopt/xforms/CXformFactory.h"

using namespace gpopt;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CSearchStage::CSearchStage
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSearchStage::CSearchStage
	(
	CXformSet *pxfs,
	ULONG ulTimeThreshold,
	CCost costThreshold
	)
	:
	m_pxfs(pxfs),
	m_ulTimeThreshold(ulTimeThreshold),
	m_costThreshold(costThreshold),
	m_pexprBest(NULL),
	m_costBest(GPOPT_INVALID_COST)
{
	GPOS_ASSERT(NULL != pxfs);
	GPOS_ASSERT(0 < pxfs->CElements());

}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStage::~CSearchStage
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CSearchStage::~CSearchStage()
{
	m_pxfs->Release();
	CRefCount::SafeRelease(m_pexprBest);
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStage::OsPrint
//
//	@doc:
//		Print job
//
//---------------------------------------------------------------------------
IOstream &
CSearchStage::OsPrint
	(
	IOstream &os
	)
{
	os
		<< "Search Stage" << std::endl
		<< "\ttime threshold: " << m_ulTimeThreshold
		<< ", cost threshold:" << m_costThreshold
		<< ", best plan found: " << std::endl;

	if (NULL != m_pexprBest)
	{
		os << *m_pexprBest;
	}

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CSearchStage::SetBestExpr
//
//	@doc:
//		Set best plan found at the end of search stage
//
//---------------------------------------------------------------------------
void
CSearchStage::SetBestExpr
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT_IMP(NULL != pexpr, pexpr->Pop()->FPhysical());

	m_pexprBest = pexpr;
	if (NULL != m_pexprBest)
	{
		m_costBest = m_pexprBest->Cost();
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStage::PdrgpssDefault
//
//	@doc:
//		Generate default search strategy;
//		one stage with all xforms and no time/cost thresholds
//
//---------------------------------------------------------------------------
DrgPss *
CSearchStage::PdrgpssDefault
	(
	IMemoryPool *pmp
	)
{
	CXformSet *pxfs = GPOS_NEW(pmp) CXformSet(pmp);
	pxfs->Union(CXformFactory::Pxff()->PxfsExploration());
	pxfs->Union(CXformFactory::Pxff()->PxfsImplementation());

	DrgPss *pdrgpss = GPOS_NEW(pmp) DrgPss(pmp);
	// create stage 0 with 100 jobs, and will stop further optimization if find query plan with cost < 5.0.
	CSearchStage *pStage0 = GPOS_NEW(pmp) CSearchStage(pxfs, 100, CCost(5.0));

	pxfs->AddRef();
	CSearchStage *pStage1 = GPOS_NEW(pmp) CSearchStage(pxfs);
	pdrgpss->Append(pStage0);
	pdrgpss->Append(pStage1);

	return pdrgpss;
}

// EOF


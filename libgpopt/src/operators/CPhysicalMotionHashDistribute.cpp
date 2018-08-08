//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalMotionHashDistribute.cpp
//
//	@doc:
//		Implementation of hash distribute motion operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalMotionHashDistribute.h"
#include "gpopt/base/CDistributionSpecHashedNoOp.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::CPhysicalMotionHashDistribute
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalMotionHashDistribute::CPhysicalMotionHashDistribute
	(
	IMemoryPool *mp,
	CDistributionSpecHashed *pdsHashed
	)
	:
	CPhysicalMotion(mp),
	m_pdsHashed(pdsHashed),
	m_pcrsRequiredLocal(NULL)
{
	GPOS_ASSERT(NULL != pdsHashed);
	GPOS_ASSERT(0 != pdsHashed->Pdrgpexpr()->Size());

	m_pcrsRequiredLocal = m_pdsHashed->PcrsUsed(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::~CPhysicalMotionHashDistribute
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalMotionHashDistribute::~CPhysicalMotionHashDistribute()
{
	m_pdsHashed->Release();
	m_pcrsRequiredLocal->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalMotionHashDistribute::Matches
	(
	COperator *pop
	)
	const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CPhysicalMotionHashDistribute *popHashDistribute =
			CPhysicalMotionHashDistribute::PopConvert(pop);

	return m_pdsHashed->Equals(popHashDistribute->m_pdsHashed);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalMotionHashDistribute::PcrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG child_index,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, *m_pcrsRequiredLocal);
	pcrs->Union(pcrsRequired);
	CColRefSet *pcrsChildReqd =
		PcrsChildReqd(mp, exprhdl, pcrs, child_index, gpos::ulong_max);
	pcrs->Release();

	return pcrsChildReqd;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalMotionHashDistribute::FProvidesReqdCols
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
//		CPhysicalMotionHashDistribute::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalMotionHashDistribute::EpetOrder
	(
	CExpressionHandle &, // exprhdl
	const CEnfdOrder * // peo
	)
	const
{
	return CEnfdProp::EpetRequired;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalMotionHashDistribute::PosRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &, // exprhdl
	COrderSpec *,//posInput
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif // GPOS_DEBUG
	,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	return GPOS_NEW(mp) COrderSpec(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalMotionHashDistribute::PosDerive
	(
	IMemoryPool *mp,
	CExpressionHandle & // exprhdl
	)
	const
{
	return GPOS_NEW(mp) COrderSpec(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalMotionHashDistribute::OsPrint
	(
	IOstream &os
	)
	const
{
	os << SzId() << " ";

	return m_pdsHashed->OsPrint(os);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionHashDistribute::PopConvert
//
//	@doc:
//		Conversion function
//
//---------------------------------------------------------------------------
CPhysicalMotionHashDistribute *
CPhysicalMotionHashDistribute::PopConvert
	(
	COperator *pop
	)
{
	GPOS_ASSERT(NULL != pop);
	GPOS_ASSERT(EopPhysicalMotionHashDistribute == pop->Eopid());

	return dynamic_cast<CPhysicalMotionHashDistribute*>(pop);
}			

CDistributionSpec *
CPhysicalMotionHashDistribute::PdsRequired
	(
		IMemoryPool *mp,
		CExpressionHandle &exprhdl,
		CDistributionSpec *pdsRequired,
		ULONG child_index,
		CDrvdPropArrays *pdrgpdpCtxt,
		ULONG ulOptReq
	) const
{
	CDistributionSpecHashedNoOp* pdsNoOp = dynamic_cast<CDistributionSpecHashedNoOp*>(m_pdsHashed);
	if (NULL == pdsNoOp)
	{
		return CPhysicalMotion::PdsRequired(mp, exprhdl, pdsRequired, child_index, pdrgpdpCtxt, ulOptReq);
	}
	else
	{
		CExpressionArray *pdrgpexpr = pdsNoOp->Pdrgpexpr();
		pdrgpexpr->AddRef();
		CDistributionSpecHashed* pdsHashed = GPOS_NEW(mp) CDistributionSpecHashed(pdrgpexpr, pdsNoOp->FNullsColocated());
		return pdsHashed;
	}
}

// EOF


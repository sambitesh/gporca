//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalSplit.cpp
//
//	@doc:
//		Implementation of physical split operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalSplit.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::CPhysicalSplit
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalSplit::CPhysicalSplit
	(
	IMemoryPool *mp,
	CColRefArray *pdrgpcrDelete,
	CColRefArray *pdrgpcrInsert,
	CColRef *pcrCtid,
	CColRef *pcrSegmentId,
	CColRef *pcrAction,
	CColRef *pcrTupleOid
	)
	:
	CPhysical(mp),
	m_pdrgpcrDelete(pdrgpcrDelete),
	m_pdrgpcrInsert(pdrgpcrInsert),
	m_pcrCtid(pcrCtid),
	m_pcrSegmentId(pcrSegmentId),
	m_pcrAction(pcrAction),
	m_pcrTupleOid(pcrTupleOid),
	m_pcrsRequiredLocal(NULL)
{
	GPOS_ASSERT(NULL != pdrgpcrDelete);
	GPOS_ASSERT(NULL != pdrgpcrInsert);
	GPOS_ASSERT(pdrgpcrInsert->Size() == pdrgpcrDelete->Size());
	GPOS_ASSERT(NULL != pcrCtid);
	GPOS_ASSERT(NULL != pcrSegmentId);
	GPOS_ASSERT(NULL != pcrAction);

	m_pcrsRequiredLocal = GPOS_NEW(mp) CColRefSet(mp);
	m_pcrsRequiredLocal->Include(m_pdrgpcrDelete);
	m_pcrsRequiredLocal->Include(m_pdrgpcrInsert);
	if (NULL != m_pcrTupleOid)
	{
		m_pcrsRequiredLocal->Include(m_pcrTupleOid);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::~CPhysicalSplit
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalSplit::~CPhysicalSplit()
{
	m_pdrgpcrDelete->Release();
	m_pdrgpcrInsert->Release();
	m_pcrsRequiredLocal->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PosRequired
//
//	@doc:
//		Compute required sort columns of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalSplit::PosRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &, // exprhdl
	COrderSpec *, // posRequired
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

	// return empty sort order
	return GPOS_NEW(mp) COrderSpec(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalSplit::PosDerive
	(
	IMemoryPool *, //mp,
	CExpressionHandle &exprhdl
	)
	const
{
	return PosDerivePassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSplit::EpetOrder
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

	// always force sort to be on top of split
	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//		we only compute required columns for the relational child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalSplit::PcrsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &, // exprhdl,
	CColRefSet *pcrsRequired,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif // GPOS_DEBUG
	,
	CDrvdPropArrays *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, *m_pcrsRequiredLocal);
	pcrs->Union(pcrsRequired);
	pcrs->Exclude(m_pcrAction);

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSplit::PdsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &, // exprhdl,
	CDistributionSpec *, // pdsInput,
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

 	return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalSplit::PrsRequired
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

	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PppsRequired
//
//	@doc:
//		Compute required partition propagation of the n-th child
//
//---------------------------------------------------------------------------
CPartitionPropagationSpec *
CPhysicalSplit::PppsRequired
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl,
	CPartitionPropagationSpec *pppsRequired,
	ULONG child_index,
	CDrvdPropArrays *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);
	GPOS_ASSERT(NULL != pppsRequired);

	return CPhysical::PppsRequiredPushThru(mp, exprhdl, pppsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalSplit::PcteRequired
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
//		CPhysicalSplit::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalSplit::FProvidesReqdCols
	(
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(NULL != pcrsRequired);
	GPOS_ASSERT(2 == exprhdl.Arity());

	CColRefSet *pcrs = GPOS_NEW(m_mp) CColRefSet(m_mp);
	// include defined column
	pcrs->Include(m_pcrAction);

	// include output columns of the relational child
	pcrs->Union(exprhdl.GetRelationalProperties(0 /*child_index*/)->PcrsOutput());

	BOOL fProvidesCols = pcrs->ContainsAll(pcrsRequired);
	pcrs->Release();

	return fProvidesCols;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSplit::PdsDerive
	(
	IMemoryPool *mp,
	CExpressionHandle &exprhdl
	)
	const
{
	CDistributionSpec *pdsOuter = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
	
	if (CDistributionSpec::EdtHashed != pdsOuter->Edt())
	{
		pdsOuter->AddRef();
		return pdsOuter;
	}
	
	// find out which columns of the target table get modified by the DML and check
	// whether those participate in the derived hash distribution
	CColRefSet *pcrsModified = GPOS_NEW(mp) CColRefSet(mp);
	const ULONG num_cols = m_pdrgpcrDelete->Size();
	
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *pcrOld = (*m_pdrgpcrDelete)[ul];
		CColRef *new_colref = (*m_pdrgpcrInsert)[ul];
		if (pcrOld != new_colref)
		{
			pcrsModified->Include(pcrOld);
			pcrsModified->Include(new_colref);
		}
	}
	
	CDistributionSpecHashed *pdsHashed = CDistributionSpecHashed::PdsConvert(pdsOuter);
	CColRefSet *pcrsHashed = CUtils::PcrsExtractColumns(mp, pdsHashed->Pdrgpexpr());
	
	if (!pcrsModified->IsDisjoint(pcrsHashed))
	{
		pcrsModified->Release();
		pcrsHashed->Release();
		return GPOS_NEW(mp) CDistributionSpecRandom();
	}
		
	if (NULL != pdsHashed->PdshashedEquiv())
	{
		CColRefSet *pcrsHashedEquiv = CUtils::PcrsExtractColumns(mp, pdsHashed->PdshashedEquiv()->Pdrgpexpr());
		if (!pcrsModified->IsDisjoint(pcrsHashedEquiv))
		{
			pcrsHashed->Release();
			pcrsHashedEquiv->Release();
			pcrsModified->Release();
			return GPOS_NEW(mp) CDistributionSpecRandom();
		}
		pcrsHashedEquiv->Release();
	}
	
	pcrsModified->Release();
	pcrsHashed->Release();
	pdsHashed->AddRef();
	return pdsHashed;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalSplit::PrsDerive
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
//		CPhysicalSplit::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CPhysicalSplit::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), CUtils::UlHashColArray(m_pdrgpcrInsert));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrCtid));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrSegmentId));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrAction));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::Matches
//
//	@doc:
//		Match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalSplit::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() == Eopid())
	{
		CPhysicalSplit *popSplit = CPhysicalSplit::PopConvert(pop);

		return m_pcrCtid == popSplit->PcrCtid() &&
				m_pcrSegmentId == popSplit->PcrSegmentId() &&
				m_pcrAction == popSplit->PcrAction() &&
				m_pcrTupleOid == popSplit->PcrTupleOid() &&
				m_pdrgpcrDelete->Equals(popSplit->PdrgpcrDelete()) &&
				m_pdrgpcrInsert->Equals(popSplit->PdrgpcrInsert());
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSplit::EpetRewindability
	(
	CExpressionHandle &exprhdl,
	const CEnfdRewindability *per
	)
	const
{
	// get rewindability delivered by the split node
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		 // required rewindability is already provided
		 return CEnfdProp::EpetUnnecessary;
	}

	// always force spool to be on top of split
	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSplit::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalSplit::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os	<< SzId() << " -- Delete Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrDelete);
	os	<< "], Insert Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrInsert);
	os	<< "], ";
	m_pcrCtid->OsPrint(os);
	os	<< ", ";
	m_pcrSegmentId->OsPrint(os);
	os	<< ", Action: ";
	m_pcrAction->OsPrint(os);

	return os;
}


// EOF

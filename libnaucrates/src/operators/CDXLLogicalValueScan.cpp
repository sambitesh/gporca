//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CDXLLogicalValueScan.cpp
//
//	@doc:
//		Implementation of DXL logical Value scan
//		
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLLogicalValueScan.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"


using namespace gpos;
using namespace gpdxl;

// Ctor
CDXLLogicalValueScan::CDXLLogicalValueScan
	(
	IMemoryPool *pmp,
	DrgPdxlcd *pdrgdxlcd,
	DrgPdrgPul *pdrgpdrgpul,
	BOOL fCastAcrossInputs
	)
	:
	CDXLLogical(pmp),
	m_pdrgpdxlcd(pdrgdxlcd),
	m_pdrgpdrgpul(pdrgpdrgpul),
	m_fCastAcrossInputs(fCastAcrossInputs)
{
	GPOS_ASSERT(NULL != m_pdrgpdxlcd);
	GPOS_ASSERT(NULL != m_pdrgpdrgpul);
	
#ifdef GPOS_DEBUG	
	const ULONG ulCols = m_pdrgpdxlcd->UlLength();
	const ULONG ulLen = m_pdrgpdrgpul->UlLength();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		DrgPul *pdrgpulInput = (*m_pdrgpdrgpul)[ul];
		GPOS_ASSERT(ulCols == pdrgpulInput->UlLength());
	}

#endif	
}

// Dtor
CDXLLogicalValueScan::~CDXLLogicalValueScan()
{
	m_pdrgpdxlcd->Release();
	m_pdrgpdrgpul->Release();
}


// Operator type
Edxlopid
CDXLLogicalValueScan::Edxlop() const
{
	return EdxlopLogicalValueScan;
}


// Operator name
const CWStringConst *
CDXLLogicalValueScan::PstrOpName() const
{
		return CDXLTokens::PstrToken(EdxltokenLogicalValueScan);
}

// Serialize operator in DXL format
void
CDXLLogicalValueScan::SerializeToDXL
	(
	CXMLSerializer *pxmlser,
	const CDXLNode *pdxln
	)
	const
{
	const CWStringConst *pstrElemName = PstrOpName();
	pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), pstrElemName);

	// serialize the array of input colid arrays
	CWStringDynamic *pstrInputColIds = CDXLUtils::PstrSerialize(m_pmp, m_pdrgpdrgpul);
	pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenInputCols), pstrInputColIds);
	GPOS_DELETE(pstrInputColIds);
	
	pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenCastAcrossInputs), m_fCastAcrossInputs);

	// serialize output columns
	pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenColumns));
	GPOS_ASSERT(NULL != m_pdrgpdxlcd);

	const ULONG ulLen = m_pdrgpdxlcd->UlLength();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		CDXLColDescr *pdxlcd = (*m_pdrgpdxlcd)[ul];
		pdxlcd->SerializeToDXL(pxmlser);
	}
	pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenColumns));

	// serialize children
	pdxln->SerializeChildrenToDXL(pxmlser);

	pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), pstrElemName);
}

// Check if given column is defined by operator
BOOL
CDXLLogicalValueScan::FDefinesColumn
	(
	ULONG ulColId
	)
	const
{
	const ULONG ulSize = UlArity();
	for (ULONG ulDescr = 0; ulDescr < ulSize; ulDescr++)
	{
		ULONG ulId = Pdxlcd(ulDescr)->UlID();
		if (ulId == ulColId)
		{
			return true;
		}
	}

	return false;
}

// conversion function
CDXLLogicalValueScan *
CDXLLogicalValueScan::PdxlopConvert
	(
	CDXLOperator *pdxlop
	)
{
	GPOS_ASSERT(NULL != pdxlop);
	GPOS_ASSERT(EdxlopLogicalValueScan == pdxlop->Edxlop());
	
	return dynamic_cast<CDXLLogicalValueScan*>(pdxlop);
}

#ifdef GPOS_DEBUG

// Checks whether operator node is well-structured
void
CDXLLogicalValueScan::AssertValid
	(
	const CDXLNode *pdxln,
	BOOL fValidateChildren
	) const
{
	GPOS_ASSERT(NULL != m_pdrgpdxlcd);

	// validate output columns
	const ULONG ulOutputCols = m_pdrgpdxlcd->UlLength();
	GPOS_ASSERT(0 < ulOutputCols);

	// validate children
	const ULONG ulChildren = pdxln->UlArity();
	for (ULONG ul = 0; ul < ulChildren; ++ul)
	{
		CDXLNode *pdxlnChild = (*pdxln)[ul];
		GPOS_ASSERT(EdxloptypeLogical == pdxlnChild->Pdxlop()->Edxloperatortype());

		if (fValidateChildren)
		{
			pdxlnChild->Pdxlop()->AssertValid(pdxlnChild, fValidateChildren);
		}
	}
}

#endif // GPOS_DEBUG

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerLogicalValueScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		value scan.
//
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalValueScan.h"
#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

// Ctor
CParseHandlerLogicalValueScan::CParseHandlerLogicalValueScan
	(
	IMemoryPool *pmp,
	CParseHandlerManager *pphm,
	CParseHandlerBase *pphRoot
	)
	:
	CParseHandlerLogicalOp(pmp, pphm, pphRoot),
	m_pdrgpdrgpulInputColIds(NULL),
	m_fCastAcrossInputs(false)
{
}

// Dtor
CParseHandlerLogicalValueScan::~CParseHandlerLogicalValueScan()
{
}

// Invoked by Xerces to process an opening tag
void
CParseHandlerLogicalValueScan::StartElement
	(
	const XMLCh* const xmlszUri,
	const XMLCh* const xmlszLocalname,
	const XMLCh* const xmlszQname,
	const Attributes& attrs
	)
{

	if (0 == this->UlLength())
	{
		// parse array of input colid arrays
		const XMLCh *xmlszInputColIds = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenInputCols));
		m_pdrgpdrgpulInputColIds = CDXLOperatorFactory::PdrgpdrgpulFromXMLCh(m_pphm->Pmm(), xmlszInputColIds, EdxltokenInputCols, EdxltokenLogicalValueScan);

		// install column descriptor parsers
		CParseHandlerBase *pphColDescr = CParseHandlerFactory::Pph(m_pmp, CDXLTokens::XmlstrToken(EdxltokenColumns), m_pphm, this);
		m_pphm->ActivateParseHandler(pphColDescr);

		m_fCastAcrossInputs = CDXLOperatorFactory::FValueFromAttrs(m_pphm->Pmm(), attrs, EdxltokenCastAcrossInputs, EdxltokenLogicalValueScan);

		// store child parse handler in array
		this->Append(pphColDescr);
	}
	else
	{
		// create child node parsers
		CParseHandlerBase *pphChild = CParseHandlerFactory::Pph(m_pmp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_pphm, this);
		m_pphm->ActivateParseHandler(pphChild);

		this->Append(pphChild);
		
		pphChild->startElement(xmlszUri, xmlszLocalname, xmlszQname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalValueScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalValueScan::EndElement
	(
	const XMLCh* const, // xmlszUri,
	const XMLCh* const ,
	const XMLCh* const // xmlszQname
	)
{
	const ULONG ulLen = this->UlLength();
	GPOS_ASSERT(3 <= ulLen);

	// get the columns descriptors
	CParseHandlerColDescr *pphColDescr = dynamic_cast<CParseHandlerColDescr *>((*this)[0]);
	GPOS_ASSERT(NULL != pphColDescr->Pdrgpdxlcd());
	DrgPdxlcd *pdrgpdxlcd = pphColDescr->Pdrgpdxlcd();

	pdrgpdxlcd->AddRef();
	CDXLLogicalValueScan *pdxlop = GPOS_NEW(m_pmp) CDXLLogicalValueScan(m_pmp,  pdrgpdxlcd, m_pdrgpdrgpulInputColIds, m_fCastAcrossInputs);
	m_pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	for (ULONG ul = 1; ul < ulLen; ul++)
	{
		// add constructed logical children from child parse handlers
		CParseHandlerLogicalOp *pphChild = dynamic_cast<CParseHandlerLogicalOp*>((*this)[ul]);
		AddChildFromParseHandler(pphChild);
	}		
	
#ifdef GPOS_DEBUG
	m_pdxln->Pdxlop()->AssertValid(m_pdxln, false /* fValidateChildren */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_pphm->DeactivateHandler();
}
// EOF

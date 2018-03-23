//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.

#include "unittest/dxl/CParseHandlerCostModelTest.h"

#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include "gpdbcost/CCostModelParamsGPDB.h"


#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/io/COstreamString.h"
#include "naucrates/dxl/parser/CParseHandlerCostModel.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

static BOOL
FEquals(IMemoryPool *pmp, ICostModelParams *pcmExpected, ICostModelParams *pcmActual)
{
	CWStringDynamic strActual(pmp);
	COstreamString ossActual(&strActual);
	pcmActual->OsPrint(ossActual);

	CWStringDynamic strExpected(pmp);
	COstreamString ossExpected(&strExpected);
	pcmExpected->OsPrint(ossExpected);

	return strExpected == strActual;
}

gpos::GPOS_RESULT CParseHandlerCostModelTest::EresUnittest() {
	gpos::CAutoMemoryPool amp;
	IMemoryPool *pmp = amp.Pmp();

	const XMLByte szDXL[] =
			"<dxl:CostModelConfig CostModelType=\"1\" SegmentsForCosting=\"3\" xmlns:dxl=\"http://greenplum.com/dxl/2010/12/\"/>";
	gpos::CAutoP<CDXLMemoryManager> pmm(GPOS_NEW(pmp) CDXLMemoryManager(pmp));
	SAX2XMLReader *pxmlreader = XMLReaderFactory::createXMLReader(pmm.Pt());

	CAutoP<CParseHandlerManager> pphm(GPOS_NEW(pmp) CParseHandlerManager(pmm.Pt(), pxmlreader));
	CAutoP<CParseHandlerCostModel> pphcm(GPOS_NEW(pmp) CParseHandlerCostModel(pmp, pphm.Pt(), NULL));

	pphm->ActivateParseHandler(pphcm.Pt());

	MemBufInputSource mbis(
			(const XMLByte*) szDXL,
			sizeof(szDXL),
			"dxl test",
			false,
			pmm.Pt()
	);
	pxmlreader->parse(mbis);
	delete pxmlreader;

	ICostModel *pcm = pphcm->Pcm();

	GPOS_RTL_ASSERT(ICostModel::EcmtGPDBCalibrated == pcm->Ecmt());
	GPOS_RTL_ASSERT(3 == pcm->UlHosts());

	CAutoRef<CCostModelParamsGPDB> pcpExpected(GPOS_NEW(pmp) CCostModelParamsGPDB(pmp));

	GPOS_RTL_ASSERT(FEquals(pmp, pcpExpected.Pt(), pcm->Pcp()));


	return gpos::GPOS_OK;
}

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerLogicalValueScan.h
//
//	@doc:
//		Parse handler for parsing a logical value scan
//
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalValueScan_H
#define GPDXL_CParseHandlerLogicalValueScan_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"
#include "naucrates/dxl/operators/CDXLLogicalValueScan.h"

namespace gpdxl
{
	using namespace gpos;

	XERCES_CPP_NAMESPACE_USE

	// Parse handler for parsing a logical value scan
	class CParseHandlerLogicalValueScan : public CParseHandlerLogicalOp
	{
		private:

			// array of input column id arrays
			DrgPdrgPul *m_pdrgpdrgpulInputColIds;

			// do the columns across inputs need to be casted
			BOOL m_fCastAcrossInputs;

			// private copy ctor
			CParseHandlerLogicalValueScan(const CParseHandlerLogicalValueScan &);

			// process the start of an element
			void StartElement
				(
					const XMLCh* const xmlszUri, 		// URI of element's namespace
 					const XMLCh* const xmlszLocalname,	// local part of element's name
					const XMLCh* const xmlszQname,		// element's qname
					const Attributes& attr				// element's attributes
				);

			// process the end of an element
			void EndElement
				(
					const XMLCh* const xmlszUri, 		// URI of element's namespace
					const XMLCh* const xmlszLocalname,	// local part of element's name
					const XMLCh* const xmlszQname		// element's qname
				);

		public:
			// ctor
			CParseHandlerLogicalValueScan
				(
				IMemoryPool *pmp,
				CParseHandlerManager *pphm,
				CParseHandlerBase *pphRoot
				);

			// dtor
			~CParseHandlerLogicalValueScan();
	};
}

#endif // !GPDXL_CParseHandlerLogicalValueScan_H

// EOF

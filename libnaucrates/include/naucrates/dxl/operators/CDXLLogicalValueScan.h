//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CDXLLogicalValueScan.h
//
//	@doc:
//		Class for representing DXL logical Value scan
//		
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalValueScan_H
#define GPDXL_CDXLLogicalValueScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLColDescr.h"

namespace gpdxl
{

	// Class for representing DXL logical Value Scan
	class CDXLLogicalValueScan : public CDXLLogical
	{
		private:

			// private copy ctor
			CDXLLogicalValueScan(CDXLLogicalValueScan&);

			// list of output column descriptors
			DrgPdxlcd *m_pdrgpdxlcd;

			// array of input colid arrays
			DrgPdrgPul *m_pdrgpdrgpul;
			
			// do the columns need to be casted accross inputs
			BOOL m_fCastAcrossInputs;

		public:
			// ctor
			CDXLLogicalValueScan
				(
				IMemoryPool *pmp,
				DrgPdxlcd *pdrgdxlcd,
				DrgPdrgPul *pdrgpdrgpul,
				BOOL fCastAcrossInput
				);

			// dtor
			virtual
			~CDXLLogicalValueScan();

			// operator id
			Edxlopid Edxlop() const;

			// operator name
			const CWStringConst *PstrOpName() const;

			// array of output columns
			const DrgPdxlcd *Pdrgpdxlcd() const
			{
				return m_pdrgpdxlcd;
			}

			// number of output columns
			ULONG UlArity() const
			{
				return m_pdrgpdxlcd->UlLength();
			}

			// output column descriptor at a given position
			const CDXLColDescr *Pdxlcd
				(
				ULONG ulPos
				)
				const
			{
				return (*m_pdrgpdxlcd)[ulPos];
			}

			// number of inputs to the n-ary value scan
		    ULONG UlChildren() const
			{
				return m_pdrgpdrgpul->UlLength();	
			}
		
			// column array of the input at a given position 
			const DrgPul *Pdrgpul
				(
				ULONG ulPos
				)
				const
			{
				GPOS_ASSERT(ulPos < UlChildren());
				
				return (*m_pdrgpdrgpul)[ulPos];
			}
		
			// do the columns across inputs need to be casted
			BOOL FCastAcrossInputs() const
			{
				return m_fCastAcrossInputs;
			}

			// serialize operator in DXL format
			virtual
			void SerializeToDXL(CXMLSerializer *pxmlser, const CDXLNode *pdxln) const;

			// check if given column is defined by operator
			virtual
			BOOL FDefinesColumn(ULONG ulColId) const;

			// conversion function
			static
			CDXLLogicalValueScan *PdxlopConvert(CDXLOperator *pdxlop);
			
#ifdef GPOS_DEBUG
			// checks whether the operator has valid structure, i.e. number and
			// types of child nodes
			void AssertValid(const CDXLNode *, BOOL fValidateChildren) const;
#endif // GPOS_DEBUG

	};
}
#endif // !GPDXL_CDXLLogicalValueScan_H

// EOF

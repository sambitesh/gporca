//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalNLJoin.h
//
//	@doc:
//		Class for representing DXL nested loop join operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalNLJoin_H
#define GPDXL_CDXLPhysicalNLJoin_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysicalJoin.h"

namespace gpdxl
{
	// indices of nested loop join elements in the children array
	enum Edxlnlj
	{
		EdxlnljIndexProjList = 0,
		EdxlnljIndexFilter,
		EdxlnljIndexJoinFilter,
		EdxlnljIndexLeftChild,
		EdxlnljIndexRightChild,
		EdxlnljIndexSentinel
	};

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLPhysicalNLJoin
	//
	//	@doc:
	//		Class for representing DXL nested loop join operators
	//
	//---------------------------------------------------------------------------
	class CDXLPhysicalNLJoin : public CDXLPhysicalJoin
	{
	private:
		// flag to indicate whether operator is an index nested loops,
		// i.e., inner side is an index scan that uses values from outer side
		BOOL m_is_index_nlj;

		// private copy ctor
		CDXLPhysicalNLJoin(const CDXLPhysicalNLJoin &);

	public:
		// ctor/dtor
		CDXLPhysicalNLJoin(IMemoryPool *mp, EdxlJoinType join_type, BOOL is_index_nlj);

		// accessors
		Edxlopid GetDXLOperator() const;
		const CWStringConst *GetOpNameStr() const;

		// is operator an index nested loops?
		BOOL
		IsIndexNLJ() const
		{
			return m_is_index_nlj;
		}

		// serialize operator in DXL format
		virtual void SerializeToDXL(CXMLSerializer *xml_serializer, const CDXLNode *dxlnode) const;

		// conversion function
		static CDXLPhysicalNLJoin *
		PdxlConvert(CDXLOperator *dxl_op)
		{
			GPOS_ASSERT(NULL != dxl_op);
			GPOS_ASSERT(EdxlopPhysicalNLJoin == dxl_op->GetDXLOperator());

			return dynamic_cast<CDXLPhysicalNLJoin *>(dxl_op);
		}

#ifdef GPOS_DEBUG
		// checks whether the operator has valid structure, i.e. number and
		// types of child nodes
		void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif  // GPOS_DEBUG
	};
}  // namespace gpdxl
#endif  // !GPDXL_CDXLPhysicalNLJoin_H

// EOF

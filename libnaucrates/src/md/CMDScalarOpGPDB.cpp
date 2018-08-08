//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDScalarOpGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific scalar ops
//		in the MD cache
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDScalarOpGPDB.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::CMDScalarOpGPDB
//
//	@doc:
//		Constructs a metadata scalar op
//
//---------------------------------------------------------------------------
CMDScalarOpGPDB::CMDScalarOpGPDB(IMemoryPool *mp,
								 IMDId *mdid,
								 CMDName *mdname,
								 IMDId *mdid_type_left,
								 IMDId *mdid_type_right,
								 IMDId *result_type_mdid,
								 IMDId *mdid_func,
								 IMDId *mdid_commute_opr,
								 IMDId *m_mdid_inverse_opr,
								 IMDType::ECmpType cmp_type,
								 BOOL returns_null_on_null_input,
								 IMdIdArray *mdid_op_classes_array)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_mdid_type_left(mdid_type_left),
	  m_mdid_type_right(mdid_type_right),
	  m_mdid_type_result(result_type_mdid),
	  m_func_mdid(mdid_func),
	  m_mdid_commute_opr(mdid_commute_opr),
	  m_mdid_inverse_opr(m_mdid_inverse_opr),
	  m_comparision_type(cmp_type),
	  m_returns_null_on_null_input(returns_null_on_null_input),
	  m_mdid_op_classes_array(mdid_op_classes_array)
{
	GPOS_ASSERT(NULL != mdid_op_classes_array);
	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::~CMDScalarOpGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDScalarOpGPDB::~CMDScalarOpGPDB()
{
	m_mdid->Release();
	m_mdid_type_result->Release();
	m_func_mdid->Release();

	CRefCount::SafeRelease(m_mdid_type_left);
	CRefCount::SafeRelease(m_mdid_type_right);
	CRefCount::SafeRelease(m_mdid_commute_opr);
	CRefCount::SafeRelease(m_mdid_inverse_opr);

	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
	m_mdid_op_classes_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::MDId
//
//	@doc:
//		Operator id
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::Mdname
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
CMDName
CMDScalarOpGPDB::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::GetLeftMdid
//
//	@doc:
//		Type id of left operand
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::GetLeftMdid() const
{
	return m_mdid_type_left;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::GetRightMdid
//
//	@doc:
//		Type id of right operand
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::GetRightMdid() const
{
	return m_mdid_type_right;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::GetResultTypeMdid
//
//	@doc:
//		Type id of result
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::GetResultTypeMdid() const
{
	return m_mdid_type_result;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::FuncMdId
//
//	@doc:
//		Id of function which implements the operator
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::FuncMdId() const
{
	return m_func_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::GetCommuteOpMdid
//
//	@doc:
//		Id of commute operator
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::GetCommuteOpMdid() const
{
	return m_mdid_commute_opr;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::GetInverseOpMdid
//
//	@doc:
//		Id of inverse operator
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::GetInverseOpMdid() const
{
	return m_mdid_inverse_opr;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::IsEqualityOp
//
//	@doc:
//		Is this an equality operator
//
//---------------------------------------------------------------------------
BOOL
CMDScalarOpGPDB::IsEqualityOp() const
{
	return IMDType::EcmptEq == m_comparision_type;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::ReturnsNullOnNullInput
//
//	@doc:
//		Does operator return NULL when all inputs are NULL?
//		STRICT implies NULL-returning, but the opposite is not always true,
//		the implementation in GPDB returns what STRICT property states
//
//---------------------------------------------------------------------------
BOOL
CMDScalarOpGPDB::ReturnsNullOnNullInput() const
{
	return m_returns_null_on_null_input;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::ParseCmpType
//
//	@doc:
//		Comparison type
//
//---------------------------------------------------------------------------
IMDType::ECmpType
CMDScalarOpGPDB::ParseCmpType() const
{
	return m_comparision_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::Serialize
//
//	@doc:
//		Serialize scalar op metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDScalarOpGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
								CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOp));

	m_mdid->Serialize(xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName), m_mdname->GetMDName());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOpCmpType),
								 IMDType::GetCmpTypeStr(m_comparision_type));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenReturnsNullOnNullInput),
								 m_returns_null_on_null_input);

	Edxltoken dxl_token_array[6] = {EdxltokenGPDBScalarOpLeftTypeId,
									EdxltokenGPDBScalarOpRightTypeId,
									EdxltokenGPDBScalarOpResultTypeId,
									EdxltokenGPDBScalarOpFuncId,
									EdxltokenGPDBScalarOpCommOpId,
									EdxltokenGPDBScalarOpInverseOpId};

	IMDId *mdid_array[6] = {m_mdid_type_left,
							m_mdid_type_right,
							m_mdid_type_result,
							m_func_mdid,
							m_mdid_commute_opr,
							m_mdid_inverse_opr};

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(dxl_token_array); ul++)
	{
		SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(dxl_token_array[ul]), mdid_array[ul]);

		GPOS_CHECK_ABORT;
	}

	// serialize operator class information
	if (0 < m_mdid_op_classes_array->Size())
	{
		SerializeMDIdList(xml_serializer,
						  m_mdid_op_classes_array,
						  CDXLTokens::GetDXLTokenStr(EdxltokenOpClasses),
						  CDXLTokens::GetDXLTokenStr(EdxltokenOpClass));
	}

	xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
								 CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOp));
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::OpClassesCount
//
//	@doc:
//		Number of classes this operator belongs to
//
//---------------------------------------------------------------------------
ULONG
CMDScalarOpGPDB::OpClassesCount() const
{
	return m_mdid_op_classes_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::OpClassMdidAt
//
//	@doc:
//		Operator class at given position
//
//---------------------------------------------------------------------------
IMDId *
CMDScalarOpGPDB::OpClassMdidAt(ULONG pos) const
{
	GPOS_ASSERT(pos < m_mdid_op_classes_array->Size());

	return (*m_mdid_op_classes_array)[pos];
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDScalarOpGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDScalarOpGPDB::DebugPrint(IOstream &os) const
{
	os << "Operator id: ";
	MDId()->OsPrint(os);
	os << std::endl;

	os << "Operator name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;

	os << "Left operand type id: ";
	GetLeftMdid()->OsPrint(os);
	os << std::endl;

	os << "Right operand type id: ";
	GetRightMdid()->OsPrint(os);
	os << std::endl;

	os << "Result type id: ";
	GetResultTypeMdid()->OsPrint(os);
	os << std::endl;

	os << "Operator func id: ";
	FuncMdId()->OsPrint(os);
	os << std::endl;

	os << "Commute operator id: ";
	GetCommuteOpMdid()->OsPrint(os);
	os << std::endl;

	os << "Inverse operator id: ";
	GetInverseOpMdid()->OsPrint(os);
	os << std::endl;

	os << std::endl;
}

#endif  // GPOS_DEBUG


// EOF

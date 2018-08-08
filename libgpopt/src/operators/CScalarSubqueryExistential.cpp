//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryExistential.cpp
//
//	@doc:
//		Implementation of existential subquery operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarSubqueryExistential.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::CScalarSubqueryExistential
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSubqueryExistential::CScalarSubqueryExistential
	(
	IMemoryPool *mp
	)
	:
	CScalar(mp)
{}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::~CScalarSubqueryExistential
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarSubqueryExistential::~CScalarSubqueryExistential()
{}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::MDIdType
//
//	@doc:
//		Type of scalar's value
//
//---------------------------------------------------------------------------
IMDId *
CScalarSubqueryExistential::MDIdType() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	return md_accessor->PtMDType<IMDTypeBool>()->MDId();
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarSubqueryExistential::Matches
	(
	COperator *pop
	)
	const
{
	GPOS_ASSERT(NULL != pop);

	return pop->Eopid() == Eopid();
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::PpartinfoDerive
//
//	@doc:
//		Derive partition consumers
//
//---------------------------------------------------------------------------
CPartInfo *
CScalarSubqueryExistential::PpartinfoDerive
	(
	IMemoryPool *, // mp, 
	CExpressionHandle &exprhdl
	)
	const
{
	CPartInfo *ppartinfoChild = exprhdl.GetRelationalProperties(0 /*child_index*/)->Ppartinfo();
	GPOS_ASSERT(NULL != ppartinfoChild);
	ppartinfoChild->AddRef();
	return ppartinfoChild;
}

// EOF


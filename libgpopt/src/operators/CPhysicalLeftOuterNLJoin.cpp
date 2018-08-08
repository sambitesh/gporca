//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLeftOuterNLJoin.cpp
//
//	@doc:
//		Implementation of left outer nested-loops join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CPhysicalLeftOuterNLJoin.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterNLJoin::CPhysicalLeftOuterNLJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterNLJoin::CPhysicalLeftOuterNLJoin
	(
	IMemoryPool *mp
	)
	:
	CPhysicalNLJoin(mp)
{}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterNLJoin::~CPhysicalLeftOuterNLJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterNLJoin::~CPhysicalLeftOuterNLJoin()
{}


// EOF


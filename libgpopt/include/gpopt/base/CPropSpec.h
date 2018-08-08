//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPropSpec.h
//
//	@doc:
//		Abstraction for specification of properties;
//---------------------------------------------------------------------------
#ifndef GPOPT_CPropSpec_H
#define GPOPT_CPropSpec_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/operators/CExpression.h"

namespace gpopt
{
	using namespace gpos;

	// prototypes
	class CReqdPropPlan;

	//---------------------------------------------------------------------------
	//	@class:
	//		CPropSpec
	//
	//	@doc:
	//		Property specification
	//
	//---------------------------------------------------------------------------
	class CPropSpec : public CRefCount
	{
		public:

			// property type
			enum EPropSpecType
			{
				EpstOrder,
				EpstDistribution,
				EpstRewindability,
				EpstPartPropagation,

				EpstSentinel
			};

		private:

			// private copy ctor
			CPropSpec(const CPropSpec &);

		protected:

			// ctor
			CPropSpec()
			{}

			// dtor
			~CPropSpec()
			{}

		public:

			// append enforcers to dynamic array for the given plan properties
			virtual
			void AppendEnforcers(IMemoryPool *mp, CExpressionHandle &exprhdl, CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr, CExpression *pexpr) = 0;

			// hash function
			virtual
			ULONG HashValue() const = 0;

			// extract columns used by the property
			virtual
			CColRefSet *PcrsUsed(IMemoryPool *mp) const = 0;

			// property type
			virtual
			EPropSpecType Epst() const = 0;

			// print
			virtual
			IOstream &OsPrint(IOstream &os) const = 0;
		
#ifdef GPOS_DEBUG
			// debug print for interactive debugging sessions only
			void DbgPrint() const;
#endif // GPOS_DEBUG

	}; // class CPropSpec


	// shorthand for printing
	inline
	IOstream &operator << (IOstream &os, const CPropSpec &ospec)
	{
		return ospec.OsPrint(os);
	}

}

#endif // !GPOPT_CPropSpec_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CDrvdPropCtxt.h
//
//	@doc:
//		Base class for derived properties context;
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CDrvdPropCtxt_H
#define GPOPT_CDrvdPropCtxt_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"


namespace gpopt
{
	using namespace gpos;

	// fwd declarations
	class CDrvdPropCtxt;
	class DrvdPropArray;

	// dynamic array for properties
	typedef CDynamicPtrArray<CDrvdPropCtxt, CleanupRelease> CDrvdPropCtxtArray;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDrvdPropCtxt
	//
	//	@doc:
	//		Container of information passed among expression nodes during
	//		property derivation
	//
	//---------------------------------------------------------------------------
	class CDrvdPropCtxt : public CRefCount
	{

		private:

			// private copy ctor
			CDrvdPropCtxt(const CDrvdPropCtxt &);

		protected:

			// memory pool
			IMemoryPool *m_mp;

			// copy function
			virtual
			CDrvdPropCtxt *PdpctxtCopy(IMemoryPool *mp) const = 0;

			// add props to context
			virtual
			void AddProps(DrvdPropArray *pdp) = 0;

		public:

			// ctor
			CDrvdPropCtxt
				(
				IMemoryPool *mp
				)
				:
				m_mp(mp)
			{}

			// dtor
			virtual
			~CDrvdPropCtxt() {}

#ifdef GPOS_DEBUG

			// is it a relational property context?
			virtual
			BOOL FRelational() const
			{
				return false;
			}

			// is it a plan property context?
			virtual
			BOOL FPlan() const
			{
				return false;
			}

			// is it a scalar property context?
			virtual
			BOOL FScalar()const
			{
				return false;
			}

			// debug print for interactive debugging sessions only
			void DbgPrint() const;

#endif // GPOS_DEBUG

			// print
			virtual
			IOstream &OsPrint(IOstream &os) const = 0;

			// copy function
			static
			CDrvdPropCtxt *PdpctxtCopy
				(
				IMemoryPool *mp,
				CDrvdPropCtxt *pdpctxt
				)
			{
				if (NULL == pdpctxt)
				{
					return NULL;
				}

				return pdpctxt->PdpctxtCopy(mp);
			}

			// add derived props to context
			static
			void AddDerivedProps
				(
				DrvdPropArray *pdp,
				CDrvdPropCtxt *pdpctxt
				)
			{
				if (NULL != pdpctxt)
				{
					pdpctxt->AddProps(pdp);
				}
			}

	}; // class CDrvdPropCtxt

 	// shorthand for printing
	IOstream &operator << (IOstream &os, CDrvdPropCtxt &drvdpropctxt);

}


#endif // !GPOPT_CDrvdPropCtxt_H

// EOF

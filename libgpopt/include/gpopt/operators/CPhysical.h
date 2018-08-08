//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysical.h
//
//	@doc:
//		Base class for all physical operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysical_H
#define GPOPT_CPhysical_H

#include "gpos/base.h"
#include "gpos/sync/CMutex.h"

#include "gpopt/operators/COperator.h"
#include "gpopt/base/CDrvdPropPlan.h"
#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CEnfdOrder.h"
#include "gpopt/base/CEnfdDistribution.h"
#include "gpopt/base/CEnfdRewindability.h"
#include "gpopt/base/CEnfdPartitionPropagation.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/base/CRewindabilitySpec.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/cost/CCost.h"

#define GPOPT_PLAN_PROPS	4	// number of plan properties requested during optimization,
								// currently, there are 4 properties: order, distribution, rewindability and partition propagation

namespace gpopt
{
	using namespace gpos;

	// arrays of unsigned integer arrays
	typedef CDynamicPtrArray<ULONG_PTR, CleanupDeleteArray> ULONGPtrArray;

	// forward declaration
	class CPartIndexMap;
	class CTableDescriptor;
	class CCostContext;
	class CCTEMap;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysical
	//
	//	@doc:
	//		base class for all physical operators
	//
	//---------------------------------------------------------------------------
	class CPhysical : public COperator
	{

		public:

			// the order in which operator triggers the execution of its children
			enum EChildExecOrder
			{
				EceoLeftToRight,	// children execute in left to right order
				EceoRightToLeft,  // children execute in right to left order

				EceoSentinel
			};

			enum EPropogatePartConstraint
			{
				EppcAllowed,
				EppcProhibited,

				EppcSentinel
			};

		private:

			//---------------------------------------------------------------------------
			//	@class:
			//		CReqdColsRequest
			//
			//	@doc:
			//		Representation of incoming column requests during optimization
			//
			//---------------------------------------------------------------------------
			class CReqdColsRequest : public CRefCount
			{

				private:

					// incoming required columns
					CColRefSet *m_pcrsRequired;

					// index of target physical child for which required columns need to be computed
					ULONG m_ulChildIndex;

					// index of scalar child to be used when computing required columns
					ULONG m_ulScalarChildIndex;

					// private copy ctor
					CReqdColsRequest(const CReqdColsRequest&);

				public:

					// ctor
					CReqdColsRequest
						(
						CColRefSet *pcrsRequired,
						ULONG child_index,
						ULONG ulScalarChildIndex
						)
						:
						m_pcrsRequired(pcrsRequired),
						m_ulChildIndex(child_index),
						m_ulScalarChildIndex(ulScalarChildIndex)
					{
						GPOS_ASSERT(NULL != pcrsRequired);
					}

					// dtor
					virtual
					~CReqdColsRequest()
					{
						m_pcrsRequired->Release();
					}

					// required columns
					CColRefSet *GetColRefSet() const
					{
						return m_pcrsRequired;
					}

					// child index to push requirements to
					ULONG UlChildIndex() const
					{
						return m_ulChildIndex;
					}

					// scalar child index
					ULONG UlScalarChildIndex() const
					{
						return m_ulScalarChildIndex;
					}

					// hash function
					static
					ULONG HashValue(const CReqdColsRequest *prcr);

					// equality function
					static
					BOOL Equals(const CReqdColsRequest *prcrFst, const CReqdColsRequest *prcrSnd);

			}; // class CReqdColsRequest

			// map of incoming required columns request to computed column sets
			typedef CHashMap<CReqdColsRequest, CColRefSet, CReqdColsRequest::HashValue, CReqdColsRequest::Equals,
						CleanupRelease<CReqdColsRequest>, CleanupRelease<CColRefSet> > ReqdColsReqToColRefSetMap;

			// hash map of child columns requests
			ReqdColsReqToColRefSetMap *m_phmrcr;

			// mutex for locking map of child columns requests during lookup/insertion
			CMutex m_mutex;

			// given an optimization context, the elements in this array represent is the
			// number of requests that operator will create for its child,
			// array entries correspond to order, distribution, rewindability and partition
			// propagation, respectively
			ULONG m_rgulOptReqs[GPOPT_PLAN_PROPS];

			// array of expanded requests
			ULONGPtrArray *m_pdrgpulpOptReqsExpanded;

			// total number of optimization requests
			ULONG m_ulTotalOptRequests;

			// update number of requests of a given property
			void UpdateOptRequests(ULONG ulPropIndex, ULONG ulRequests);

			// private copy ctor
			CPhysical(const CPhysical &);

			// check whether we can push a part table requirement to a given child, given
			// the knowledge of where the part index id is defined
			static
			BOOL FCanPushPartReqToChild(CBitSet *pbsPartConsumer, ULONG child_index);

		protected:

			// set number of order requests that operator creates for its child
			void SetOrderRequests
				(
				ULONG ulOrderReqs
				)
			{
				UpdateOptRequests(0 /*ulPropIndex*/, ulOrderReqs);
			}

			// set number of distribution requests that operator creates for its child
			void SetDistrRequests
				(
				ULONG ulDistrReqs
				)
			{
				UpdateOptRequests(1  /*ulPropIndex*/, ulDistrReqs);
			}

			// set number of rewindability requests that operator creates for its child
			void SetRewindRequests
				(
				ULONG ulRewindReqs
				)
			{
				UpdateOptRequests(2 /*ulPropIndex*/, ulRewindReqs);
			}

			// set number of partition propagation requests that operator creates for its child
			void SetPartPropagateRequests
				(
				ULONG ulPartPropagationReqs
				)
			{
				UpdateOptRequests(3 /*ulPropIndex*/, ulPartPropagationReqs);
			}

			// pass cte requirement to the n-th child
			CCTEReq *PcterNAry
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CCTEReq *pcter,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt
				)
				const;

			// helper for computing required columns of the n-th child by including used
			// columns and excluding defined columns of the scalar child
			CColRefSet *PcrsChildReqd
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsInput,
				ULONG child_index,
				ULONG ulScalarIndex
				);

			// compute distribution spec from the table descriptor
			static
			CDistributionSpec *PdsCompute(IMemoryPool *mp, const CTableDescriptor *ptabdesc, CColRefArray *pdrgpcrOutput);

			// helper for a simple case of computing child's required sort order
			static
			COrderSpec *PosPassThru
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				COrderSpec *posInput,
				ULONG child_index
				);

			// helper for a simple case of computing child's required distribution
			static
			CDistributionSpec *PdsPassThru
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG child_index
				);
			
			// helper for computing child's required distribution when Master-Only/Replicated
			// distributions must be requested
			static
			CDistributionSpec *PdsMasterOnlyOrReplicated
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG child_index,
				ULONG ulOptReq
				);

			// helper for computing child's required distribution in unary operators
			// with a single scalar child
			static
			CDistributionSpec *PdsUnary
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG child_index,
				ULONG ulOptReq
				);

			// helper for a simple case of computing child's required rewindability
			static
			CRewindabilitySpec *PrsPassThru
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired,
				ULONG child_index
				);

			// pass partition propagation requirement to the child
			static
			CPartitionPropagationSpec *PppsRequiredPushThru
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CPartitionPropagationSpec *pppsRequired,
				ULONG child_index
				);
			
			// pass partition propagation requirement to the children of an n-ary operator
			static
			CPartitionPropagationSpec *PppsRequiredPushThruNAry
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CPartitionPropagationSpec *pppsRequired,
				ULONG child_index
				);
			
			// helper function for pushing unresolved partition propagation in unary
			// operators
			static
			CPartitionPropagationSpec *PppsRequiredPushThruUnresolvedUnary
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CPartitionPropagationSpec *pppsRequired,
				EPropogatePartConstraint eppcPropogate
				);
			
			// pass cte requirement to the child
			static
			CCTEReq *PcterPushThru(CCTEReq *pcter);

			// combine the derived CTE maps of the first n children
			// of the given expression handle
			static
			CCTEMap *PcmCombine
				(
				IMemoryPool *mp,
				CDrvdPropArrays *pdrgpdpCtxt
				);

			// helper for common case of sort order derivation
			static
			COrderSpec *PosDerivePassThruOuter(CExpressionHandle &exprhdl);

			// helper for common case of distribution derivation
			static
			CDistributionSpec *PdsDerivePassThruOuter(CExpressionHandle &exprhdl);

			// helper for common case of rewindability derivation
			static
			CRewindabilitySpec *PrsDerivePassThruOuter(CExpressionHandle &exprhdl);

			// helper for checking if output columns of a unary operator
			// that defines no new columns include the required columns
			static
			BOOL FUnaryProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired);

			// helper for common case of passing through partition index map
			static
			CPartIndexMap *PpimPassThruOuter(CExpressionHandle &exprhdl);
			
			// helper for common case of passing through partition filter map
			static
			CPartFilterMap *PpfmPassThruOuter(CExpressionHandle &exprhdl);

			// combine derived part filter maps of relational children
			static
			CPartFilterMap *PpfmDeriveCombineRelational(IMemoryPool *mp, CExpressionHandle &exprhdl);

			// helper for common case of combining partition index maps of all relational children
			static
			CPartIndexMap *PpimDeriveCombineRelational(IMemoryPool *mp, CExpressionHandle &exprhdl);

			// enforce an operator to be executed on the master
			static
			CDistributionSpec *PdsEnforceMaster
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pds,
				ULONG child_index
				);

			// helper to compute skew estimate based on given stats and distribution spec
			static
			CDouble GetSkew(IStatistics *stats, CDistributionSpec *pds);


			// return true if the given column set includes any of the columns defined by
			// the unary node, as given by the handle
			static
			BOOL FUnaryUsesDefinedColumns(CColRefSet *pcrs, CExpressionHandle &exprhdl);

		public:
		
			// ctor
			explicit
			CPhysical(IMemoryPool *mp);

			// dtor
			virtual 
			~CPhysical()
			{
				CRefCount::SafeRelease(m_phmrcr);
				CRefCount::SafeRelease(m_pdrgpulpOptReqsExpanded);
			}

			// type of operator
			virtual
			BOOL FPhysical() const
			{
				GPOS_ASSERT(!FLogical() && !FScalar() && !FPattern());
				return true;
			}

			// create base container of derived properties
			virtual
			DrvdPropArray *PdpCreate(IMemoryPool *mp) const;

			// create base container of required properties
			virtual
			CReqdProp *PrpCreate(IMemoryPool *mp) const;

			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required output columns of the n-th child
			virtual
			CColRefSet *PcrsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				) = 0;

			// compute required ctes of the n-th child
			virtual
			CCTEReq *PcteRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CCTEReq *pcter,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const = 0;

			// compute required sort order of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				COrderSpec *posRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const = 0;

			// compute required distribution of the n-th child
			virtual
			CDistributionSpec *PdsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const = 0;

			// compute required rewindability of the n-th child
			virtual
			CRewindabilitySpec *PrsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const = 0;

			// compute required partition propagation of the n-th child
			virtual
			CPartitionPropagationSpec *PppsRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CPartitionPropagationSpec *pppsRequired,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				) = 0;
			
			// required properties: check if required columns are included in output columns
			virtual
			BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired, ULONG ulOptReq) const = 0;

			// required properties: check if required CTEs are included in derived CTE map
			virtual
			BOOL FProvidesReqdCTEs(CExpressionHandle &exprhdl, const CCTEReq *pcter) const;

			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive sort order
			virtual
			COrderSpec *PosDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const = 0;

			// dderive distribution
			virtual
			CDistributionSpec *PdsDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const = 0;

			// derived properties: derive rewindability
			virtual
			CRewindabilitySpec *PrsDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const = 0;

			// derive partition index map
			virtual
			CPartIndexMap *PpimDerive(IMemoryPool *mp, CExpressionHandle &exprhdl, CDrvdPropCtxt *pdpctxt) const = 0;

			// derive partition filter map
			virtual
			CPartFilterMap *PpfmDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const = 0;

			// derive cte map
			virtual
			CCTEMap *PcmDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			// See CEngine::FCheckEnfdProps() for comments on usage.
			//-------------------------------------------------------------------------------------

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				) const = 0;

			// return distribution property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetDistribution
				(
				CExpressionHandle &exprhdl,
				const CEnfdDistribution *ped
				) const;

			// return rewindability property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetRewindability
				(
				CExpressionHandle &exprhdl,
				const CEnfdRewindability *per
				) const = 0;
			
			// return partition propagation property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetPartitionPropagation
				(
				CExpressionHandle &exprhdl,
				const CEnfdPartitionPropagation *pepp
				) 
				const;

			// distribution matching type
			virtual
			CEnfdDistribution::EDistributionMatching Edm
				(
				CReqdPropPlan *prppInput,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				);

			// order matching type
			virtual
			CEnfdOrder::EOrderMatching Eom
				(
				CReqdPropPlan *prppInput,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				);
			
			// rewindability matching type
			virtual
			CEnfdRewindability::ERewindabilityMatching Erm
				(
				CReqdPropPlan *prppInput,
				ULONG child_index,
				CDrvdPropArrays *pdrgpdpCtxt,
				ULONG ulOptReq
				);

			// check if optimization contexts is valid
			virtual
			BOOL FValidContext
				(
				IMemoryPool *, // mp
				COptimizationContext *, // poc,
				COptimizationContextArray * // pdrgpocChild
				)
				const
			{
				return true;
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// execution order of children
			virtual
			EChildExecOrder Eceo() const
			{
				// by default, children execute in left to right order
				return EceoLeftToRight;
			}

			// number of order requests that operator creates for its child
			ULONG UlOrderRequests() const
			{
				return m_rgulOptReqs[0];
			}

			// number of distribution requests that operator creates for its child
			ULONG UlDistrRequests() const
			{
				return m_rgulOptReqs[1];
			}

			// number of rewindability requests that operator creates for its child
			ULONG UlRewindRequests() const
			{
				return m_rgulOptReqs[2];
			}

			// number of partition propagation requests that operator creates for its child
			ULONG UlPartPropagateRequests() const
			{
				return m_rgulOptReqs[3];
			}

			// return total number of optimization requests
			ULONG UlOptRequests() const
			{
				return m_ulTotalOptRequests;
			}

			// map request number to order, distribution, rewindability and partition propagation requests
			void LookupRequest
				(
				ULONG ulReqNo, // input: request number
				ULONG *pulOrderReq, // output: order request number
				ULONG *pulDistrReq, // output: distribution request number
				ULONG *pulRewindReq, // output: rewindability request number
				ULONG *pulPartPropagateReq // output: partition propagation request number
				);

			// return true if operator passes through stats obtained from children,
			// this is used when computing stats during costing
			virtual
			BOOL FPassThruStats() const = 0;

			// true iff the delivered distributions of the children are compatible among themselves
			virtual
			BOOL FCompatibleChildrenDistributions(const CExpressionHandle &exprhdl) const;

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

			// conversion function
			static
			CPhysical *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(pop->FPhysical());

				return dynamic_cast<CPhysical*>(pop);
			}

			// helper for computing a singleton distribution matching the given distribution
			static
			CDistributionSpecSingleton *PdssMatching(IMemoryPool *mp, CDistributionSpecSingleton *pdss);

	}; // class CPhysical

}


#endif // !GPOPT_CPhysical_H

// EOF

//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (C) 2019 Pivotal Inc.
//
//	@filename:
//		CJoinOrderDPv2.cpp
//
//	@doc:
//		Implementation of dynamic programming-based join order generation
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpos/common/clibwrapper.h"
#include "gpos/common/CBitSet.h"
#include "gpos/common/CBitSetIter.h"

#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CNormalizer.h"
#include "gpopt/operators/CScalarNAryJoinPredList.h"
#include "gpopt/xforms/CJoinOrderDPv2.h"

#include "gpopt/exception.h"

#include "naucrates/statistics/CJoinStatsProcessor.h"


using namespace gpopt;

#define GPOPT_DPV2_JOIN_ORDERING_TOPK 10
#define GPOPT_DPV2_CROSS_JOIN_PENALTY 5


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::CJoinOrderDPv2
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CJoinOrderDPv2::CJoinOrderDPv2
	(
	CMemoryPool *mp,
	CExpressionArray *pdrgpexprAtoms,
	CExpressionArray *innerJoinConjuncts,
	CExpressionArray *onPredConjuncts,
	ULongPtrArray *childPredIndexes
	)
	:
	CJoinOrder(mp, pdrgpexprAtoms, innerJoinConjuncts, onPredConjuncts, childPredIndexes),
	m_expression_to_edge_map(NULL),
	m_on_pred_conjuncts(onPredConjuncts),
	m_child_pred_indexes(childPredIndexes),
	m_non_inner_join_dependencies(NULL)
{
	m_join_levels = GPOS_NEW(mp) DPv2Levels(mp, m_ulComps+1);
	// populate levels array with n+1 levels for an n-way join
	// level 0 remains unused, so index i corresponds to level i,
	// making it easier for humans to read the code
	for (ULONG l=0; l<= m_ulComps; l++)
	{
		m_join_levels->Append(GPOS_NEW(mp) SLevelInfo(l, GPOS_NEW(mp) SGroupInfoArray(mp)));
	}

	m_bitset_to_group_info_map = GPOS_NEW(mp) BitSetToGroupInfoMap(mp);

	m_top_k_expressions = GPOS_NEW(mp) KHeap<SExpressionInfoArray, SExpressionInfo>
										(
										 mp,
										 this,
										 GPOPT_DPV2_JOIN_ORDERING_TOPK
										);

	m_mp = mp;
	if (0 < m_on_pred_conjuncts->Size())
	{
		// we have non-inner joins, add dependency info
		ULONG numNonInnerJoins = m_on_pred_conjuncts->Size();

		m_non_inner_join_dependencies = GPOS_NEW(mp) CBitSetArray(mp, numNonInnerJoins);
		for (ULONG ul=0; ul<numNonInnerJoins; ul++)
		{
			m_non_inner_join_dependencies->Append(GPOS_NEW(mp) CBitSet(mp));
		}

		// compute dependencies of the NIJ right children
		// (those components must appear on the left of the NIJ)
		// Note: NIJ = Non-inner join, e.g. LOJ
		for (ULONG en = 0; en < m_ulEdges; en++)
		{
			SEdge *pedge = m_rgpedge[en];

			if (0 < pedge->m_loj_num)
			{
				// edge represents a non-inner join pred
				ULONG logicalChildNum = FindLogicalChildByNijId(pedge->m_loj_num);
				CBitSet * nijBitSet = (*m_non_inner_join_dependencies)[pedge->m_loj_num-1];

				GPOS_ASSERT(0 < logicalChildNum);
				nijBitSet->Union(pedge->m_pbs);
				// clear the bit representing the right side of the NIJ, we only
				// want to track the components needed on the left side
				nijBitSet->ExchangeClear(logicalChildNum);
			}
		}
		PopulateExpressionToEdgeMapIfNeeded();
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::~CJoinOrderDPv2
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CJoinOrderDPv2::~CJoinOrderDPv2()
{
#ifdef GPOS_DEBUG
	// in optimized build, we flush-down memory pools without leak checking,
	// we can save time in optimized build by skipping all de-allocations here,
	// we still have all de-llocations enabled in debug-build to detect any possible leaks
	CRefCount::SafeRelease(m_non_inner_join_dependencies);
	CRefCount::SafeRelease(m_child_pred_indexes);
	m_bitset_to_group_info_map->Release();
	CRefCount::SafeRelease(m_expression_to_edge_map);
	m_top_k_expressions->Release();
	m_join_levels->Release();
	m_on_pred_conjuncts->Release();
#endif // GPOS_DEBUG
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::ComputeCost
//
//	@doc:
//		Primitive costing of join expressions;
//		Cost of a join expression is the "internal data flow" of the join
//		tree, the sum of all the rows flowing from the leaf nodes up to
//		the root.
//		NOTE: We could consider the width of the rows as well, if we had
//		a reliable way of determining the actual width.
//
//---------------------------------------------------------------------------
void
CJoinOrderDPv2::ComputeCost
	(
	 SExpressionInfo *expr_info,
	 CDouble join_cardinality
	)
{
	// cardinality of the expression itself is one part of the cost
	CDouble dCost(join_cardinality);

	if (expr_info->m_left_child_expr.IsValid())
	{
		GPOS_ASSERT(expr_info->m_right_child_expr.IsValid());
		// add cardinalities of the children to the cost
		dCost = dCost + expr_info->m_left_child_expr.GetExprInfo()->m_cost;
		dCost = dCost + expr_info->m_right_child_expr.GetExprInfo()->m_cost;

		if (CUtils::FCrossJoin(expr_info->m_expr))
		{
			// penalize cross joins, similar to what we do in the optimization phase
			dCost = dCost * GPOPT_DPV2_CROSS_JOIN_PENALTY;
		}
	}

	expr_info->m_cost = dCost;
}

//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::PexprBuildInnerJoinPred
//
//	@doc:
//		Build predicate connecting the two given sets
//
//---------------------------------------------------------------------------
CExpression *
CJoinOrderDPv2::PexprBuildInnerJoinPred
	(
	CBitSet *pbsFst,
	CBitSet *pbsSnd
	)
{
	GPOS_ASSERT(pbsFst->IsDisjoint(pbsSnd));
	// collect edges connecting the given sets
	CBitSet *pbsEdges = GPOS_NEW(m_mp) CBitSet(m_mp);
	CBitSet *pbs = GPOS_NEW(m_mp) CBitSet(m_mp, *pbsFst);
	pbs->Union(pbsSnd);

	for (ULONG ul = 0; ul < m_ulEdges; ul++)
	{
		SEdge *pedge = m_rgpedge[ul];
		if (
			// edge represents an inner join pred
			0 == pedge->m_loj_num &&
			// all columns referenced in the edge pred are provided
			pbs->ContainsAll(pedge->m_pbs) &&
			// the edge represents a true join predicate between the two components
			!pbsFst->IsDisjoint(pedge->m_pbs) &&
			!pbsSnd->IsDisjoint(pedge->m_pbs)
			)
		{
#ifdef GPOS_DEBUG
		BOOL fSet =
#endif // GPOS_DEBUG
			pbsEdges->ExchangeSet(ul);
			GPOS_ASSERT(!fSet);
		}
	}
	pbs->Release();

	CExpression *pexprPred = NULL;
	if (0 < pbsEdges->Size())
	{
		CExpressionArray *pdrgpexpr = GPOS_NEW(m_mp) CExpressionArray(m_mp);
		CBitSetIter bsi(*pbsEdges);
		while (bsi.Advance())
		{
			ULONG ul = bsi.Bit();
			SEdge *pedge = m_rgpedge[ul];
			pedge->m_pexpr->AddRef();
			pdrgpexpr->Append(pedge->m_pexpr);
		}

		pexprPred = CPredicateUtils::PexprConjunction(m_mp, pdrgpexpr);
	}

	pbsEdges->Release();
	return pexprPred;
}

void CJoinOrderDPv2::DeriveStats(CExpression *pexpr)
{
	try {
		// We want to let the histogram code compute the join selectivity and the number of NDVs based
		// on actual histogram buckets, taking into account the overlap of the data ranges. It helps
		// with getting more consistent and accurate cardinality estimates for DP.
		// Eventually, this should probably become the default method.
		CJoinStatsProcessor::SetComputeScaleFactorFromHistogramBuckets(true);
		CJoinOrder::DeriveStats(pexpr);
		CJoinStatsProcessor::SetComputeScaleFactorFromHistogramBuckets(false);
	} catch (...) {
		CJoinStatsProcessor::SetComputeScaleFactorFromHistogramBuckets(false);
		throw;
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::GetJoinExprForProperties
//
//	@doc:
//		Build a CExpression joining the two given sets, choosing child
//		expressions with given properties
//
//---------------------------------------------------------------------------
CJoinOrderDPv2::SExpressionInfo *
CJoinOrderDPv2::GetJoinExprForProperties
	(
	 SGroupInfo *left_child,
	 SGroupInfo *right_child,
	 SExpressionProperties &required_properties
	)
{
	SGroupAndExpression left_expr = GetBestExprForProperties(left_child, required_properties);
	SGroupAndExpression right_expr = GetBestExprForProperties(right_child, required_properties);

	if (!left_expr.IsValid() || !right_expr.IsValid())
	{
		return NULL;
	}

	return GetJoinExpr(left_expr, right_expr, required_properties);
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::GetJoinExpr
//
//	@doc:
//		Build a CExpression joining the two given sets from given expressions
//
//---------------------------------------------------------------------------
CJoinOrderDPv2::SExpressionInfo *
CJoinOrderDPv2::GetJoinExpr
(
 const SGroupAndExpression &left_child_expr,
 const SGroupAndExpression &right_child_expr,
 SExpressionProperties &result_properties
)
{
	SGroupInfo *left_group_info      = left_child_expr.m_group_info;
	SExpressionInfo *left_expr_info  = left_child_expr.GetExprInfo();
	SGroupInfo *right_group_info     = right_child_expr.m_group_info;
	SExpressionInfo *right_expr_info = right_child_expr.GetExprInfo();

	CExpression *scalar_expr = NULL;
	CBitSet *required_on_left = NULL;
	BOOL isNIJ = IsRightChildOfNIJ(right_group_info, &scalar_expr, &required_on_left);

	if (!isNIJ)
	{
		// inner join, compute the predicate from the join graph
		GPOS_ASSERT(NULL == scalar_expr);
		scalar_expr = PexprBuildInnerJoinPred(left_group_info->m_atoms, right_group_info->m_atoms);
	}
	else
	{
		// check whether scalar_expr can be computed from left_child and right_child,
		// otherwise this is not a valid join
		GPOS_ASSERT(NULL != scalar_expr && NULL != required_on_left);
		if (!left_group_info->m_atoms->ContainsAll(required_on_left))
		{
			// the left child does not produce all the values needed in the ON
			// predicate, so this is not a valid join
			return NULL;
		}
		scalar_expr->AddRef();
	}

	if (NULL == scalar_expr)
	{
		// this is a cross product

		if (right_group_info->IsAnAtom())
		{
			// generate a TRUE boolean expression as the join predicate of the cross product
			scalar_expr = CPredicateUtils::PexprConjunction(m_mp, NULL /*pdrgpexpr*/);
		}
		else
		{
			// we don't do bushy cross products, any mandatory or optional cross products
			// are linear trees
			return NULL;
		}
	}

	CExpression *join_expr = NULL;

	CExpression *left_expr = left_expr_info->m_expr;
	CExpression *right_expr = right_expr_info->m_expr;
	left_expr->AddRef();
	right_expr->AddRef();

	if (isNIJ)
	{
		join_expr = CUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>(m_mp, left_expr, right_expr, scalar_expr);
	}
	else
	{
		join_expr = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(m_mp, left_expr, right_expr, scalar_expr);
	}

	return GPOS_NEW(m_mp) SExpressionInfo(join_expr, left_child_expr, right_child_expr, result_properties);
}


BOOL CJoinOrderDPv2::IsASupersetOfProperties(SExpressionProperties &prop, SExpressionProperties &other_prop)
{
	// are the bits in other_prop a subset of the bits in prop?
	return 0 == (other_prop.m_join_order & ~prop.m_join_order);
}

BOOL CJoinOrderDPv2::ArePropertiesDisjoint(SExpressionProperties &prop, SExpressionProperties &other_prop)
{
	return	!IsASupersetOfProperties(prop, other_prop) && !IsASupersetOfProperties(other_prop, prop);
}


CJoinOrderDPv2::SGroupAndExpression CJoinOrderDPv2::GetBestExprForProperties(SGroupInfo *group_info, SExpressionProperties &props)
{
	ULONG best_ix = gpos::ulong_max;
	CDouble best_cost(0.0);

	for (ULONG ul=0; ul < group_info->m_best_expr_info_array->Size(); ul++)
	{
		SExpressionInfo *expr_info = (*group_info->m_best_expr_info_array)[ul];

		if (IsASupersetOfProperties(expr_info->m_properties, props))
		{
			if (gpos::ulong_max == best_ix || expr_info->m_cost < best_cost)
			{
				// we found a candidate with the best cost so far that satisfies the properties
				best_ix = ul;
				best_cost = expr_info->m_cost;
			}
		}
	}

	return SGroupAndExpression(group_info, best_ix);
}


void CJoinOrderDPv2::AddNewPropertyToExpr(SGroupAndExpression expr, SExpressionProperties props)
{
	SExpressionInfo *expr_info = expr.GetExprInfo();

	expr_info->m_properties.Add(props);
}


void
CJoinOrderDPv2::AddExprToGroupIfNecessary(SGroupInfo *group_info, SExpressionInfo *new_expr_info)
{
	// compute the cost for the new expression
	ComputeCost(new_expr_info, group_info->m_cardinality);
	CDouble new_cost = new_expr_info->m_cost;

	if (group_info->m_atoms->Size() == m_ulComps)
	{
		// At the top level, we have only one group. To be able to return multiple results
		// for the xform, we keep the top k expressions (all from the same group) in a KHeap
		new_expr_info->AddRef();
		m_top_k_expressions->Insert(new_expr_info);
	}

	if (0 == group_info->m_best_expr_info_array->Size() || new_cost < group_info->m_lowest_expr_cost)
	{
		// update the low water mark for the cost seen in this group
		group_info->m_lowest_expr_cost = new_cost;
	}

	// is there another expression already that dominates this one?
	SGroupAndExpression existing_expr = GetBestExprForProperties(group_info, new_expr_info->m_properties);

	if (!existing_expr.IsValid())
	{
		// this expression provides new properties, insert it
		group_info->m_best_expr_info_array->Append(new_expr_info);
	}
	else
	{
		SExpressionInfo *existing_expr_info = existing_expr.GetExprInfo();

		// we found an existing expression that satisfies the properties, now check whether
		// we should keep the existing one, the new one, or both
		if (new_cost < existing_expr_info->m_cost)
		{
			// our new expression is cheaper, now check whether it provides all the properties of the existing one
			if (IsASupersetOfProperties(new_expr_info->m_properties, existing_expr_info->m_properties))
			{
				// yes, the new expression dominates the existing one (provides same or better properties for lower cost)
				// replace the existing one with the new one
				group_info->m_best_expr_info_array->Replace(existing_expr.m_expr_index, new_expr_info);
			}
			else
			{
				// the new expression is cheaper, but it provides less than the existing one, so keep both
				group_info->m_best_expr_info_array->Append(new_expr_info);
			}
		}
		else
		{
			// the new expression does not provide any properties for the lowest cost, so discard it
			new_expr_info->Release();
		}
	}
}


BOOL
CJoinOrderDPv2::PopulateExpressionToEdgeMapIfNeeded()
{
	// In some cases we may not place all of the predicates in the NAry join in
	// the resulting tree of binary joins. If that situation is a possibility,
	// we'll create a map from expressions to edges, so that we can find any
	// unused edges to be placed in a select node on top of the join.
	//
	// Example:
	// select * from foo left join bar on foo.a=bar.a where coalesce(bar.b, 0) < 10;
	if (0 == m_child_pred_indexes->Size())
	{
		// all inner joins, all predicates will be placed
		return false;
	}

	BOOL populate = false;
	// make a bitset b with all the LOJ right children
	CBitSet *loj_right_children = GPOS_NEW(m_mp) CBitSet(m_mp);

	for (ULONG c=0; c<m_child_pred_indexes->Size(); c++)
	{
		if (0 < *((*m_child_pred_indexes)[c]))
		{
			loj_right_children->ExchangeSet(c);
		}
	}

	for (ULONG en1 = 0; en1 < m_ulEdges; en1++)
	{
		SEdge *pedge = m_rgpedge[en1];

		if (pedge->m_loj_num == 0)
		{
			// check whether this inner join (WHERE) predicate refers to any LOJ right child
			// (whether its bitset overlaps with b)
			// or whether we see any local predicates (this should be uncommon)
			if (!loj_right_children->IsDisjoint(pedge->m_pbs) || 1 == pedge->m_pbs->Size())
			{
				populate = true;
				break;
			}
		}
	}

	if (populate)
	{
		m_expression_to_edge_map = GPOS_NEW(m_mp) ExpressionToEdgeMap(m_mp);

		for (ULONG en2 = 0; en2 < m_ulEdges; en2++)
		{
			SEdge *pedge = m_rgpedge[en2];

			pedge->AddRef();
			pedge->m_pexpr->AddRef();
			m_expression_to_edge_map->Insert(pedge->m_pexpr, pedge);
		}
	}

	loj_right_children->Release();

	return populate;
}

// add a select node with any remaining edges (predicates) that have
// not been incorporated in the join tree
CExpression *
CJoinOrderDPv2::AddSelectNodeForRemainingEdges(CExpression *join_expr)
{
	if (NULL == m_expression_to_edge_map)
	{
		return join_expr;
	}

	CExpressionArray *exprArray = GPOS_NEW(m_mp) CExpressionArray(m_mp);
	RecursivelyMarkEdgesAsUsed(join_expr);

	// find any unused edges and add them to a select
	for (ULONG en = 0; en < m_ulEdges; en++)
	{
		SEdge *pedge = m_rgpedge[en];

		if (pedge->m_fUsed)
		{
			// mark the edge as unused for the next alternative, where
			// we will have to repeat this check
			pedge->m_fUsed = false;
		}
		else
		{
			// found an unused edge, this one will need to go into
			// a select node on top of the join
			pedge->m_pexpr->AddRef();
			exprArray->Append(pedge->m_pexpr);
		}
	}

	if (0 < exprArray->Size())
	{
		CExpression *conj = CPredicateUtils::PexprConjunction(m_mp, exprArray);

		return GPOS_NEW(m_mp) CExpression(m_mp, GPOS_NEW(m_mp) CLogicalSelect(m_mp), join_expr, conj);
	}

	exprArray->Release();

	return join_expr;
}


void CJoinOrderDPv2::RecursivelyMarkEdgesAsUsed(CExpression *expr)
{
	GPOS_CHECK_STACK_SIZE;

	if (expr->Pop()->FLogical())
	{
		for (ULONG ul=0; ul< expr->Arity(); ul++)
		{
			RecursivelyMarkEdgesAsUsed((*expr)[ul]);
		}
	}
	else
	{
		GPOS_ASSERT(expr->Pop()->FScalar());
		const SEdge *edge = m_expression_to_edge_map->Find(expr);
		if (NULL != edge)
		{
			// we found the edge belonging to this expression, terminate the recursion
			const_cast<SEdge *>(edge)->m_fUsed = true;
			return;
		}

		// we should not reach the leaves of the tree without finding an edge
		GPOS_ASSERT(0 < expr->Arity());

		// this is not an edge, it is probably an AND of multiple edges
		for (ULONG ul = 0; ul < expr->Arity(); ul++)
		{
			RecursivelyMarkEdgesAsUsed((*expr)[ul]);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::SearchJoinOrders
//
//	@doc:
//		Enumerate all the possible joins between two lists of components
//
//---------------------------------------------------------------------------
void
CJoinOrderDPv2::SearchJoinOrders
	(
	 ULONG left_level,
	 ULONG right_level
	)
{
	GPOS_ASSERT(left_level > 0 &&
				right_level > 0 &&
				left_level + right_level <= m_ulComps);

	SGroupInfoArray *left_group_info_array = GetGroupsForLevel(left_level);
	SGroupInfoArray *right_group_info_array = GetGroupsForLevel(right_level);
	SLevelInfo *current_level_info = Level(left_level+right_level);

	ULONG left_size = left_group_info_array->Size();
	ULONG right_size = right_group_info_array->Size();

	for (ULONG left_ix=0; left_ix<left_size; left_ix++)
	{
		SGroupInfo *left_group_info = (*left_group_info_array)[left_ix];
		CBitSet *left_bitset = left_group_info->m_atoms;
		ULONG right_ix = 0;

		// if pairs from the same level, start from the next
		// entry to avoid duplicate join combinations
		// i.e a join b and b join a, just try one
		// commutativity will take care of the other
		if (left_level == right_level)
		{
			right_ix = left_ix + 1;
		}

		for (; right_ix<right_size; right_ix++)
		{
			SGroupInfo *right_group_info = (*right_group_info_array)[right_ix];
			CBitSet *right_bitset = right_group_info->m_atoms;

			if (!left_bitset->IsDisjoint(right_bitset))
			{
				// not a valid join, left and right tables must not overlap
				continue;
			}

			SExpressionProperties reqd_properties(EJoinOrderAny);
			SExpressionInfo *join_expr_info = GetJoinExprForProperties(left_group_info, right_group_info, reqd_properties);

			if (NULL != join_expr_info)
			{
				// we have a valid join
				CBitSet *join_bitset = GPOS_NEW(m_mp) CBitSet(m_mp, *left_bitset);

				// TODO: Reduce non-mandatory cross products

				join_bitset->Union(right_bitset);

				SGroupInfo *group_info = LookupOrCreateGroupInfo(current_level_info, join_bitset, join_expr_info);

				AddExprToGroupIfNecessary(group_info, join_expr_info);
			}
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::GreedySearchJoinOrders
//
//	@doc:
//		Enumerate all the possible joins between a list of groups and the
//		list of atoms, only add the best new expression. Note that this
//		method is used for query and mincard join orders
//
//---------------------------------------------------------------------------
void
CJoinOrderDPv2::GreedySearchJoinOrders
(
 ULONG left_level,
 JoinOrderPropType algo
)
{
	ULONG right_level = 1;
	GPOS_ASSERT(left_level > 0 &&
				left_level + right_level <= m_ulComps);

	SGroupInfoArray *left_group_info_array = GetGroupsForLevel(left_level);
	SGroupInfoArray *right_group_info_array = GetGroupsForLevel(right_level);
	SLevelInfo *current_level_info = Level(left_level+right_level);
	SExpressionProperties left_reqd_properties(algo);
	SExpressionProperties right_reqd_properties(EJoinOrderAny);
	SExpressionProperties result_properties(algo);

	ULONG left_size = left_group_info_array->Size();
	ULONG right_size = right_group_info_array->Size();

	// pre-existing greedy solution on level left_level
	CBitSet *left_bitset = NULL;
	SGroupAndExpression left_child_expr_info;

	ULONG left_ix = 0;
	ULONG right_ix = 0;

	// the solution on level left_level+1 that we want to build
	SGroupInfo *best_group_info_in_level = NULL;
	SExpressionInfo *best_expr_info_in_level = NULL;
	CDouble best_cost_in_level(-1.0);

	// find the solution for the left side
	while (left_ix < left_size)
	{
		left_child_expr_info = GetBestExprForProperties((*left_group_info_array)[left_ix], left_reqd_properties);

		if (left_child_expr_info.IsValid())
		{
			left_bitset = left_child_expr_info.m_group_info->m_atoms;
			// we found the one solution from the lower level that we will build upon
			break;
		}
		left_ix++;
	}

	if (left_ix >= left_size)
	{
		// we didn't find a greedy solution for the left side
		GPOS_ASSERT(0);
		return;
	}

	if (EJoinOrderQuery == algo)
	{
		// for query, we want to pick the atoms in sequence, indexes 0 ... n-1
		right_ix = left_level;
	}

	// now loop over all the atoms on the right and pick the one we want to use for this level
	for (; right_ix<right_size; right_ix++)
	{
		SGroupInfo *right_group_info = (*right_group_info_array)[right_ix];
		CBitSet *right_bitset = right_group_info->m_atoms;

		if (!left_bitset->IsDisjoint(right_bitset))
		{
			// not a valid join, left and right tables must not overlap
			continue;
		}

		SGroupAndExpression right_child_expr_info = GetBestExprForProperties(right_group_info, right_reqd_properties);

		if (!right_child_expr_info.IsValid())
		{
			continue;
		}

		SExpressionInfo *join_expr_info = GetJoinExpr(left_child_expr_info, right_child_expr_info, result_properties);

		if (NULL != join_expr_info)
		{
			// we have a valid join
			CBitSet *join_bitset = GPOS_NEW(m_mp) CBitSet(m_mp, *left_bitset);

			join_bitset->Union(right_bitset);

			// look up existing group and stats or create a new group and derive stats
			SGroupInfo *join_group_info = LookupOrCreateGroupInfo(current_level_info, join_bitset, join_expr_info);

			ComputeCost(join_expr_info, join_group_info->m_cardinality);
			CDouble join_cost = join_expr_info->m_cost;

			if (NULL == best_expr_info_in_level || join_cost < best_cost_in_level)
			{
				best_group_info_in_level = join_group_info;
				CRefCount::SafeRelease(best_expr_info_in_level);
				best_expr_info_in_level  = join_expr_info;
				best_cost_in_level       = join_cost;
			}
			else
			{
				join_expr_info->Release();
			}

			if (EJoinOrderQuery == algo)
			{
				// we are done, we try only a single right index for join order query
				break;
			}
		}
	}

	if (NULL != best_expr_info_in_level)
	{
		// add the best expression from the loop with the specified properties
		// also add it to top k if we are at the top
		best_expr_info_in_level->m_properties.Add(algo);
		AddExprToGroupIfNecessary(best_group_info_in_level, best_expr_info_in_level);
	}
	else
	{
		// we should always find a greedy solution
		GPOS_ASSERT(0);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::LookupOrCreateGroupInfo
//
//	@doc:
//		Look up a group from a given set of atoms. If found, return it.
//		If not found, create a new group in the specified level.
//		Note that this method consumes a RefCount on <atoms> but it does
//		not consume refcounts from <levelInfo> or <stats_expr_info>.
//
//---------------------------------------------------------------------------
CJoinOrderDPv2::SGroupInfo *
CJoinOrderDPv2::LookupOrCreateGroupInfo(SLevelInfo *levelInfo, CBitSet *atoms, SExpressionInfo *stats_expr_info)
{
	SGroupInfo *group_info = m_bitset_to_group_info_map->Find(atoms);
	SExpressionInfo *real_expr_info_for_stats = stats_expr_info;

	if (NULL == group_info)
	{
		// this is a group we haven't seen yet, create a new group info and derive stats, if needed
		group_info = GPOS_NEW(m_mp) SGroupInfo(m_mp, atoms);
		if (!stats_expr_info->m_properties.Satisfies(EJoinOrderStats))
		{
			SExpressionProperties stats_props(EJoinOrderStats);

			// need to derive stats, make sure we use an expression whose children already have stats
			real_expr_info_for_stats = GetJoinExprForProperties
											(
											 stats_expr_info->m_left_child_expr.m_group_info,
											 stats_expr_info->m_right_child_expr.m_group_info,
											 stats_props
											);

			DeriveStats(real_expr_info_for_stats->m_expr);
		}
		else
		{
			GPOS_ASSERT(NULL != real_expr_info_for_stats->m_expr->Pstats());
			// we are using stats_expr_info in the new group, but the caller didn't
			// allocate a ref count for us, so add one here
			stats_expr_info->AddRef();
		}

		group_info->m_cardinality = real_expr_info_for_stats->m_expr->Pstats()->Rows();
		AddExprToGroupIfNecessary(group_info, real_expr_info_for_stats);

		if (NULL == levelInfo->m_top_k_groups)
		{
			// no limits, just add the group to the array
			// note that the groups won't be sorted by cost in this case
			levelInfo->m_groups->Append(group_info);
		}
		else
		{
			// insert into the KHeap for now, the best groups will be transferred to
			// levelInfo->m_groups when we call FinalizeDPLevel()
			levelInfo->m_top_k_groups->Insert(group_info);
		}

		if (1 < levelInfo->m_level)
		{
			// also insert into the bitset to group map
			group_info->m_atoms->AddRef();
			group_info->AddRef();
			m_bitset_to_group_info_map->Insert(group_info->m_atoms, group_info);
		}
	}
	else
	{
		atoms->Release();
	}

	return group_info;
}


void
CJoinOrderDPv2::FinalizeDPLevel(ULONG level)
{
	GPOS_ASSERT(level >= 2);
	SLevelInfo *level_info = Level(level);

	if (NULL != level_info->m_top_k_groups)
	{
		SGroupInfo *winner;

		while (NULL != (winner = level_info->m_top_k_groups->RemoveBestElement()))
		{
			// add the next best group to the level array, sorted by ascending cost
			level_info->m_groups->Append(winner);
		}

		SGroupInfo *loser;

		// also remove the groups that didn't make it from the bitset to group info map
		while (NULL != (loser = level_info->m_top_k_groups->RemoveNextElement()))
		{
			m_bitset_to_group_info_map->Delete(loser->m_atoms);
			loser->Release();
		}

		// release the remaining groups at this time, they won't be needed anymore
		level_info->m_top_k_groups->Release();
		level_info->m_top_k_groups = NULL;
	}
}



//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::SearchBushyJoinOrders
//
//	@doc:
//		Generate all bushy join trees of level current_level,
//		given an array of an array of bit sets (components), arranged by level
//
//---------------------------------------------------------------------------
void
CJoinOrderDPv2::SearchBushyJoinOrders
	(
	 ULONG current_level
	)
{
	// try bushy joins of bitsets of level x and y, where
	// x + y = current_level and x > 1 and y > 1
	// note that join trees of level 3 and below are never bushy,
	// so this loop only executes at current_level >= 4
	for (ULONG left_level = 2; left_level < current_level-1; left_level++)
	{
		if (LevelIsFull(current_level))
		{
			// we've exceeded the number of joins for which we generate bushy trees
			// TODO: Transition off of bushy joins more gracefully, note that bushy
			// trees usually do't add any more groups, they just generate more
			// expressions for existing groups
			return;
		}

		ULONG right_level = current_level - left_level;
		if (left_level > right_level)
			// we've already considered the commuted join
			break;
		SearchJoinOrders(left_level, right_level);
	}

	return;
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::PExprExpand
//
//	@doc:
//		Main driver for join order enumeration, called by xform
//
//---------------------------------------------------------------------------
void
CJoinOrderDPv2::PexprExpand()
{
	// put the "atoms", the nodes of the join tree that
	// are not joins themselves, at the first level
	SLevelInfo *atom_level = Level(1);

	// the atoms all have stats derived
	SExpressionProperties atom_props(EJoinOrderStats);

	// populate level 1 with the atoms (the logical children of the NAry join)
	for (ULONG atom_id = 0; atom_id < m_ulComps; atom_id++)
	{
		CBitSet *atom_bitset = GPOS_NEW(m_mp) CBitSet(m_mp);
		atom_bitset->ExchangeSet(atom_id);
		CExpression *pexpr_atom = m_rgpcomp[atom_id]->m_pexpr;
		pexpr_atom->AddRef();
		SExpressionInfo *atom_expr_info = GPOS_NEW(m_mp) SExpressionInfo(pexpr_atom, atom_props);

		if (0 == atom_id)
		{
			// this is the level 1 solution for the query join order
			atom_expr_info->m_properties.Add(EJoinOrderQuery);
		}

		LookupOrCreateGroupInfo(atom_level, atom_bitset, atom_expr_info);
		// note that for atoms with stats, the above call will also insert atom_expr_info as first (and only)
		// expression into the group
		atom_expr_info->Release();
	}

	// TODO: Based on optimizer_join_order, call a subset of these
	EnumerateDP();
	EnumerateQuery();
	EnumerateMinCard();
}


void
CJoinOrderDPv2::EnumerateDP()
{
	COptimizerConfig *optimizer_config = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
	const CHint *phint = optimizer_config->GetHint();
	ULONG join_order_exhaustive_limit = phint->UlJoinOrderDPLimit();

	// for larger joins, compute the limit for the number of groups at each level, this
	// follows the number of groups for the largest join for which we do exhaustive search
	if (join_order_exhaustive_limit < m_ulComps)
	{
		for (ULONG l=2; l<=m_ulComps; l++)
		{
			ULONG number_of_allowed_groups = 0;

			if (l < join_order_exhaustive_limit)
			{
				// at lower levels, limit the number of groups to that of an
				// <join_order_exhaustive_limit>-way join
				number_of_allowed_groups = NChooseK(join_order_exhaustive_limit, l);
			}
			else
			{
				// beyond that, use greedy (keep only one group per level)
				number_of_allowed_groups = 1;
			}

			// add a KHeap to this level, so that we can collect the k best expressions
			// while we are building the level
			Level(l)->m_top_k_groups = GPOS_NEW(m_mp) KHeap<SGroupInfoArray, SGroupInfo>
																	 (
																	  m_mp,
																	  this,
																	  number_of_allowed_groups
																	 );
		}
	}

	// build n-ary joins from the bottom up, starting with 2-way, 3-way up to m_ulComps-way
	for (ULONG current_join_level = 2; current_join_level <= m_ulComps; current_join_level++)
	{
		// build linear joins, with a "current_join_level"-1-way join on one
		// side and an atom on the other side
		SearchJoinOrders(current_join_level-1, 1);

		// build bushy trees - joins between two other joins
		SearchBushyJoinOrders(current_join_level);

		// finalize level, enforce limit for groups
		FinalizeDPLevel(current_join_level);
	}
}


void
CJoinOrderDPv2::EnumerateQuery()
{
	for (ULONG current_join_level = 2; current_join_level <= m_ulComps; current_join_level++)
	{
		GreedySearchJoinOrders(current_join_level-1, EJoinOrderQuery);
	}
}


void
CJoinOrderDPv2::EnumerateMinCard()
{
	// call SearchJoinOrders(1,1); if not already done elsewhere

	// find the starting pair
	SLevelInfo *level_2 = Level(2);
	CDouble min_card(0.0);
	SGroupInfo *min_card_group = NULL;
	SExpressionProperties any_props(EJoinOrderAny);

	// loop over all the 2-way joins and find the one with the lowest cardinality
	for (ULONG ul=0; ul<level_2->m_groups->Size(); ul++)
	{
		SGroupInfo *group_2 = (*level_2->m_groups)[ul];

		if (NULL == min_card_group || group_2->m_cardinality < min_card)
		{
			min_card = group_2->m_cardinality;
			min_card_group = group_2;
		}
	}

	// mark the lowest cardinality 2-way join as the MinCard solution
	SGroupAndExpression min_card_2_way_join = GetBestExprForProperties(min_card_group, any_props);

	AddNewPropertyToExpr(min_card_2_way_join, SExpressionProperties(EJoinOrderMincard));

	for (ULONG current_join_level = 3; current_join_level <= m_ulComps; current_join_level++)
	{
		GreedySearchJoinOrders(current_join_level-1, EJoinOrderMincard);
	}
}


CExpression*
CJoinOrderDPv2::GetNextOfTopK()
{
	SExpressionInfo *join_result_info = m_top_k_expressions->RemoveBestElement();

	if (NULL == join_result_info)
	{
		return NULL;
	}

	CExpression *join_result = join_result_info->m_expr;

	join_result->AddRef();
	join_result_info->Release();

	return AddSelectNodeForRemainingEdges(join_result);
}


BOOL
CJoinOrderDPv2::IsRightChildOfNIJ
	(SGroupInfo *groupInfo,
	 CExpression **onPredToUse,
	 CBitSet **requiredBitsOnLeft
	)
{
	*onPredToUse = NULL;
	*requiredBitsOnLeft = NULL;

	if (1 != groupInfo->m_atoms->Size() || 0 == m_on_pred_conjuncts->Size())
	{
		// this is not a non-join vertex component (and only those can be right
		// children of NIJs), or the entire NAry join doesn't contain any NIJs
		return false;
	}

	// get the child predicate index for the non-join vertex component represented
	// by this component
	CBitSetIter iter(*groupInfo->m_atoms);

	// there is only one bit set for this component
	iter.Advance();

	ULONG childPredIndex = *(*m_child_pred_indexes)[iter.Bit()];

	if (GPOPT_ZERO_INNER_JOIN_PRED_INDEX != childPredIndex)
	{
		// this non-join vertex component is the right child of an
		// NIJ, return the ON predicate to use and also return TRUE
		*onPredToUse = (*m_on_pred_conjuncts)[childPredIndex-1];
		// also return the required minimal component on the left side of the join
		*requiredBitsOnLeft = (*m_non_inner_join_dependencies)[childPredIndex-1];
		return true;
	}

	// this is a non-join vertex component that is not the right child of an NIJ
	return false;
}


ULONG
CJoinOrderDPv2::FindLogicalChildByNijId(ULONG nij_num)
{
	GPOS_ASSERT(NULL != m_child_pred_indexes);

	for (ULONG c=0; c<m_child_pred_indexes->Size(); c++)
	{
		if (*(*m_child_pred_indexes)[c] == nij_num)
		{
			return c;
		}
	}

	return 0;
}


ULONG CJoinOrderDPv2::NChooseK(ULONG n, ULONG k)
{
	ULLONG numerator = 1;
	ULLONG denominator = 1;

	for (ULONG i=1; i<=k; i++)
	{
		numerator *= n+1-i;
		denominator *= i;
	}

	return (ULONG) (numerator / denominator);
}


BOOL
CJoinOrderDPv2::LevelIsFull(ULONG level)
{
	SLevelInfo *li = Level(level);

	if (NULL == li->m_top_k_groups)
	{
		return false;
	}

	return li->m_top_k_groups->IsLimitExceeded();
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderDPv2::OsPrint
//
//	@doc:
//		Print created join order
//
//---------------------------------------------------------------------------
IOstream &
CJoinOrderDPv2::OsPrint
	(
	IOstream &os
	)
	const
{
	// increase GPOS_LOG_MESSAGE_BUFFER_SIZE in file ILogger.h if the output of this method gets truncated
	ULONG num_levels = m_join_levels->Size();
	ULONG num_bitsets = 0;
	CPrintPrefix pref(NULL, "      ");

	for (ULONG lev=1; lev<num_levels; lev++)
	{
		SGroupInfoArray *bitsets_this_level = GetGroupsForLevel(lev);
		ULONG num_bitsets_this_level = bitsets_this_level->Size();

		os << "CJoinOrderDPv2 - Level: " << lev << " (" << bitsets_this_level->Size() << " group(s))" << std::endl;

		for (ULONG c=0; c<num_bitsets_this_level; c++)
		{
			SGroupInfo *gi = (*bitsets_this_level)[c];
			ULONG num_exprs = gi->m_best_expr_info_array->Size();
			SExpressionProperties stats_properties(EJoinOrderStats);
			SGroupAndExpression expr_for_stats = const_cast<CJoinOrderDPv2 *>(this)->GetBestExprForProperties(gi, stats_properties);

			num_bitsets++;
			os << "   Group: ";
			gi->m_atoms->OsPrint(os);
			os << std::endl;

			if (expr_for_stats.IsValid())
			{
				os << "   Rows: " << expr_for_stats.GetExprInfo()->m_expr->Pstats()->Rows() << std::endl;
				// uncomment this for more detailed debugging
				// os << "   Expr for stats:" << std::endl;
				// expr_for_stats->OsPrint(os, &pref);
				// os << std::endl;
			}

			for (ULONG x=0; x<num_exprs; x++)
			{
				SExpressionInfo *expr_info = (*gi->m_best_expr_info_array)[x];

				os << "   Expression with properties ";
				OsPrintProperty(os, expr_info->m_properties);

				if (!gi->IsAnAtom())
				{
					os << "   Child groups: ";
					expr_info->m_left_child_expr.m_group_info->m_atoms->OsPrint(os);
					if (COperator::EopLogicalLeftOuterJoin == expr_info->m_expr->Pop()->Eopid())
					{
						os << " left";
					}
					os << " join ";
					expr_info->m_right_child_expr.m_group_info->m_atoms->OsPrint(os);
					os << std::endl;
				}
				os << "   Cost: ";
				expr_info->m_cost.OsPrint(os);
				os << std::endl;
				if (lev == 1)
				{
					os << "   Atom: " << std::endl;
					expr_info->m_expr->OsPrint(os, &pref);
				}
				else if (lev < num_levels-1)
				{
					os << "   Join predicate: " << std::endl;
					(*expr_info->m_expr)[2]->OsPrint(os, &pref);
				}
				else
				{
					os << "   Top-level expression: " << std::endl;
					expr_info->m_expr->OsPrint(os, &pref);
				}

				os << std::endl;
			}
		}
	}

	os << "CJoinOrderDPv2 - total number of groups: " << num_bitsets << std::endl;

	return os;
}


IOstream &
CJoinOrderDPv2::OsPrintProperty(IOstream &os, SExpressionProperties &props) const
{
	os << "{ ";
	if (0 == props.m_join_order)
	{
		os << "DP";
	}
	else
	{
		BOOL is_first = true;

		if (props.Satisfies(EJoinOrderQuery))
		{
			os << "Query";
			is_first = false;
		}
		if (props.Satisfies(EJoinOrderMincard))
		{
			if (!is_first)
				os << ", ";
			os << "Mincard";
			is_first = false;
		}
		if (props.Satisfies(EJoinOrderStats))
		{
			if (!is_first)
				os << ", ";
			os << "Stats";
		}
	}
	os << " }";

	return os;
}


#ifdef GPOS_DEBUG
void
CJoinOrderDPv2::DbgPrint()
{
	CAutoTrace at(m_mp);

	OsPrint(at.Os());
}
#endif


// EOF

/**
 * @file dual_tree_traverser_impl.hpp
 * @author Ryan Curtin
 *
 * A dual-tree traverser for the cover tree.
 */
#ifndef __MLPACK_CORE_TREE_COVER_TREE_DUAL_TREE_TRAVERSER_IMPL_HPP
#define __MLPACK_CORE_TREE_COVER_TREE_DUAL_TREE_TRAVERSER_IMPL_HPP

#include <mlpack/core.hpp>
#include <queue>

namespace mlpack {
namespace tree {

//! The object placed in the map for tree traversal.
template<typename MetricType, typename RootPointPolicy, typename StatisticType>
struct DualCoverTreeMapEntry
{
  //! The node this entry refers to.
  CoverTree<MetricType, RootPointPolicy, StatisticType>* referenceNode;
  //! The score of the node.
  double score;
  //! The index of the reference node used for the base case evaluation.
  size_t referenceIndex;
  //! The index of the query node used for the base case evaluation.
  size_t queryIndex;
  //! The base case evaluation.
  double baseCase;

  //! Comparison operator, for sorting within the map.
  bool operator<(const DualCoverTreeMapEntry& other) const
  {
    return (score < other.score);
  }
};

template<typename MetricType, typename RootPointPolicy, typename StatisticType>
template<typename RuleType>
CoverTree<MetricType, RootPointPolicy, StatisticType>::
DualTreeTraverser<RuleType>::DualTreeTraverser(RuleType& rule) :
    rule(rule),
    numPrunes(0)
{ /* Nothing to do. */ }

template<typename MetricType, typename RootPointPolicy, typename StatisticType>
template<typename RuleType>
void CoverTree<MetricType, RootPointPolicy, StatisticType>::
DualTreeTraverser<RuleType>::Traverse(
    CoverTree<MetricType, RootPointPolicy, StatisticType>& queryNode,
    CoverTree<MetricType, RootPointPolicy, StatisticType>& referenceNode)
{
  typedef DualCoverTreeMapEntry<MetricType, RootPointPolicy, StatisticType>
      MapEntryType;

  // Start by creating a map and adding the reference node to it.
  std::map<int, std::vector<MapEntryType> > refMap;

  MapEntryType rootRefEntry;

  rootRefEntry.referenceNode = &referenceNode;
  rootRefEntry.score = 0.0; // Must recurse into.
  rootRefEntry.referenceIndex = referenceNode.Point();
  rootRefEntry.queryIndex = queryNode.Point();
  rootRefEntry.baseCase = rule.BaseCase(queryNode.Point(),
      referenceNode.Point());
//  rule.UpdateAfterRecursion(queryNode, referenceNode);

  refMap[referenceNode.Scale()].push_back(rootRefEntry);

  Traverse(queryNode, refMap);
}

template<typename MetricType, typename RootPointPolicy, typename StatisticType>
template<typename RuleType>
void CoverTree<MetricType, RootPointPolicy, StatisticType>::
DualTreeTraverser<RuleType>::Traverse(
  CoverTree<MetricType, RootPointPolicy, StatisticType>& queryNode,
  std::map<int, std::vector<DualCoverTreeMapEntry<MetricType, RootPointPolicy,
      StatisticType> > >& referenceMap)
{
//  Log::Debug << "Recursed into query node " << queryNode.Point() << ", scale "
//      << queryNode.Scale() << "\n";

  // Convenience typedef.
  typedef DualCoverTreeMapEntry<MetricType, RootPointPolicy, StatisticType>
      MapEntryType;

  if (referenceMap.size() == 0)
    return; // Nothing to do!

  // First recurse down the reference nodes as necessary.
  ReferenceRecursion(queryNode, referenceMap);

  // Now, reduce the scale of the query node by recursing.  But we can't recurse
  // if the query node is a leaf node.
  if ((queryNode.Scale() != INT_MIN) &&
      (queryNode.Scale() >= (*referenceMap.rbegin()).first))
  {
    // Recurse into the non-self-children first.  The recursion order cannot
    // affect the runtime of the algorithm, because each query child recursion's
    // results are separate and independent.
    for (size_t i = 1; i < queryNode.NumChildren(); ++i)
    {
      std::map<int, std::vector<MapEntryType> > childMap;
      PruneMap(queryNode, queryNode.Child(i), referenceMap, childMap);

//      Log::Debug << "Recurse into query child " << i << ": " <<
//          queryNode.Child(i).Point() << " scale " << queryNode.Child(i).Scale()
//          << "; this parent is " << queryNode.Point() << " scale " <<
//          queryNode.Scale() << std::endl;
      Traverse(queryNode.Child(i), childMap);
    }

    PruneMapForSelfChild(queryNode.Child(0), referenceMap);

    // Now we can use the existing map (without a copy) for the self-child.
//    Log::Warn << "Recurse into query self-child " << queryNode.Child(0).Point()
//        << " scale " << queryNode.Child(0).Scale() << "; this parent is "
//        << queryNode.Point() << " scale " << queryNode.Scale() << std::endl;
    Traverse(queryNode.Child(0), referenceMap);
  }

  if (queryNode.Scale() != INT_MIN)
    return; // No need to evaluate base cases at this level.  It's all done.

  // If we have made it this far, all we have is a bunch of base case
  // evaluations to do.
  Log::Assert((*referenceMap.begin()).first == INT_MIN);
  Log::Assert(queryNode.Scale() == INT_MIN);
  std::vector<MapEntryType>& pointVector = (*referenceMap.begin()).second;
//  Log::Debug << "Onto base case evaluations\n";

  for (size_t i = 0; i < pointVector.size(); ++i)
  {
    // Get a reference to the frame.
    const MapEntryType& frame = pointVector[i];

    CoverTree<MetricType, RootPointPolicy, StatisticType>* refNode =
        frame.referenceNode;
    const double oldScore = frame.score;
    const size_t refIndex = frame.referenceIndex;
    const size_t queryIndex = frame.queryIndex;
//    Log::Debug << "Consider query " << queryNode.Point() << ", reference "
//        << refNode->Point() << "\n";
//    Log::Debug << "Old score " << oldScore << " with refParent " << refIndex
//        << " and parent query node " << queryIndex << "\n";

    // First, ensure that we have not already calculated the base case.
    if ((refIndex == refNode->Point()) && (queryIndex == queryNode.Point()))
    {
//      Log::Debug << "Pruned because we already did the base case and its value "
//          << " was " << frame.baseCase << std::endl;
      ++numPrunes;
      continue;
    }

    // Now, check if we can prune it.
    const double rescore = rule.Rescore(queryNode, *refNode, oldScore);

    if (rescore == DBL_MAX)
    {
//      Log::Debug << "Pruned after rescoring\n";
      ++numPrunes;
      continue;
    }

    // If not, compute the base case.
//    Log::Debug << "Not pruned, performing base case " << queryNode.Point() <<
//        " " << pointVector[i].referenceNode->Point() << "\n";
    rule.BaseCase(queryNode.Point(), pointVector[i].referenceNode->Point());
  }
}

template<typename MetricType, typename RootPointPolicy, typename StatisticType>
template<typename RuleType>
void CoverTree<MetricType, RootPointPolicy, StatisticType>::
DualTreeTraverser<RuleType>::PruneMap(
    CoverTree& /* queryNode */,
    CoverTree& candidateQueryNode,
    std::map<int, std::vector<DualCoverTreeMapEntry<MetricType,
        RootPointPolicy, StatisticType> > >& referenceMap,
    std::map<int, std::vector<DualCoverTreeMapEntry<MetricType,
        RootPointPolicy, StatisticType> > >& childMap)
{
//  Log::Debug << "Prep for recurse into query child point " <<
//      candidateQueryNode.Point() << " scale " <<
//      candidateQueryNode.Scale() << std::endl;
  typedef DualCoverTreeMapEntry<MetricType, RootPointPolicy, StatisticType>
      MapEntryType;

  if (referenceMap.empty())
    return; // Nothing to do.
  typename std::map<int, std::vector<MapEntryType> >::reverse_iterator it =
      referenceMap.rbegin();

  while ((it != referenceMap.rend()) && ((*it).first != INT_MIN))
  {
    // Get a reference to the vector representing the entries at this scale.
    const std::vector<MapEntryType>& scaleVector = (*it).second;

    const int thisScale = (*it).first;
    childMap[thisScale].reserve(scaleVector.size());
    std::vector<MapEntryType>& newScaleVector = childMap[thisScale];
//    newScaleVector.reserve(scaleVector.size()); // Maximum possible size.

    // Loop over each entry in the vector.
    for (size_t j = 0; j < scaleVector.size(); ++j)
    {
      const MapEntryType& frame = scaleVector[j];

      // First evaluate if we can prune without performing the base case.
      CoverTree<MetricType, RootPointPolicy, StatisticType>* refNode =
          frame.referenceNode;
      const double oldScore = frame.score;

      // Try to prune based on shell().  This is hackish and will need to be
      // refined or cleaned at some point.
//      double score = rule.PrescoreQ(queryNode, candidateQueryNode, *refNode,
//          frame.baseCase);

//      if (score == DBL_MAX)
//      {
//        ++numPrunes;
//        continue;
//      }

//      Log::Debug << "Recheck reference node " << refNode->Point() <<
//          " scale " << refNode->Scale() << " which has old score " <<
//          oldScore << " with old reference index " << frame.referenceIndex
//          << " and old query index " << frame.queryIndex << std::endl;

      double score = rule.Rescore(candidateQueryNode, *refNode, oldScore);

//      Log::Debug << "Rescored as " << score << std::endl;

      if (score == DBL_MAX)
      {
        // Pruned.  Move on.
        ++numPrunes;
        continue;
      }

      // Evaluate base case.
//      Log::Debug << "Must evaluate base case "
//          << candidateQueryNode.Point() << " " << refNode->Point()
//          << "\n";
      double baseCase = rule.BaseCase(candidateQueryNode.Point(),
          refNode->Point());
//      Log::Debug << "Base case was " << baseCase << std::endl;

      score = rule.Score(candidateQueryNode, *refNode, baseCase);

      if (score == DBL_MAX)
      {
        // Pruned.  Move on.
        ++numPrunes;
        continue;
      }

      // Add to child map.
      newScaleVector.push_back(frame);
      newScaleVector.back().score = score;
      newScaleVector.back().baseCase = baseCase;
      newScaleVector.back().referenceIndex = refNode->Point();
      newScaleVector.back().queryIndex = candidateQueryNode.Point();
    }

    // If we didn't add anything, then strike this vector from the map.
    if (newScaleVector.size() == 0)
      childMap.erase((*it).first);

    ++it; // Advance to next scale.
  }

  childMap[INT_MIN] = referenceMap[INT_MIN];
}

template<typename MetricType, typename RootPointPolicy, typename StatisticType>
template<typename RuleType>
void CoverTree<MetricType, RootPointPolicy, StatisticType>::
DualTreeTraverser<RuleType>::PruneMapForSelfChild(
    CoverTree& candidateQueryNode,
    std::map<int, std::vector<DualCoverTreeMapEntry<MetricType, RootPointPolicy,
        StatisticType> > >& referenceMap)
{
//  Log::Debug << "Prep for recurse into query self-child point " <<
//      candidateQueryNode.Point() << " scale " <<
//      candidateQueryNode.Scale() << std::endl;
  typedef DualCoverTreeMapEntry<MetricType, RootPointPolicy, StatisticType>
      MapEntryType;

  // Create the child reference map.  We will do this by recursing through
  // every entry in the reference map and evaluating (or pruning) it.  But
  // in this setting we do not recurse into any children of the reference
  // entries.
  if (referenceMap.empty())
    return; // Nothing to do.
  typename std::map<int, std::vector<MapEntryType> >::reverse_iterator it =
      referenceMap.rbegin();

  while (it != referenceMap.rend() && (*it).first != INT_MIN)
  {
    // Get a reference to the vector representing the entries at this scale.
    std::vector<MapEntryType>& newScaleVector = (*it).second;
    const std::vector<MapEntryType> scaleVector = newScaleVector;

    newScaleVector.clear();
    newScaleVector.reserve(scaleVector.size());

    // Loop over each entry in the vector.
    for (size_t j = 0; j < scaleVector.size(); ++j)
    {
      const MapEntryType& frame = scaleVector[j];

      // First evaluate if we can prune without performing the base case.
      CoverTree<MetricType, RootPointPolicy, StatisticType>* refNode =
        frame.referenceNode;
      const double oldScore = frame.score;
      double baseCase = frame.baseCase;
      const size_t queryIndex = frame.queryIndex;
      const size_t refIndex = frame.referenceIndex;

//      Log::Debug << "Recheck reference node " << refNode->Point() << " scale "
//          << refNode->Scale() << " which has old score " << oldScore
//          << std::endl;

      // Have we performed the base case yet?
      double score;
      if ((refIndex != refNode->Point()) ||
          (queryIndex != candidateQueryNode.Point()))
      {
        // Attempt to rescore before performing the base case.
        score = rule.Rescore(candidateQueryNode, *refNode, oldScore);

        if (score == DBL_MAX)
        {
          ++numPrunes;
          continue;
        }

        // If not pruned, we have to perform the base case.
        baseCase = rule.BaseCase(candidateQueryNode.Point(), refNode->Point());
      }

      score = rule.Score(candidateQueryNode, *refNode, baseCase);

//      Log::Debug << "Rescored as " << score << std::endl;

      if (score == DBL_MAX)
      {
        // Pruned.  Move on.
        ++numPrunes;
        continue;
      }

//      Log::Debug << "Kept in map\n";

      // Add to child map.
      newScaleVector.push_back(frame);
      newScaleVector.back().score = score;
      newScaleVector.back().baseCase = baseCase;
      newScaleVector.back().queryIndex = candidateQueryNode.Point();
      newScaleVector.back().referenceIndex = refNode->Point();
    }

    // If we didn't add anything, then strike this vector from the map.
    if (newScaleVector.size() == 0)
    {
      referenceMap.erase((*it).first);
      if (referenceMap.empty())
        break;
    }

    ++it; // Advance to next scale.
  }
}

template<typename MetricType, typename RootPointPolicy, typename StatisticType>
template<typename RuleType>
void CoverTree<MetricType, RootPointPolicy, StatisticType>::
DualTreeTraverser<RuleType>::ReferenceRecursion(
    CoverTree& queryNode,
    std::map<int, std::vector<DualCoverTreeMapEntry<MetricType, RootPointPolicy,
        StatisticType> > >& referenceMap)
{
  typedef DualCoverTreeMapEntry<MetricType, RootPointPolicy, StatisticType>
      MapEntryType;

  // First, reduce the maximum scale in the reference map down to the scale of
  // the query node.
  while ((*referenceMap.rbegin()).first > queryNode.Scale())
  {
    // If the query node's scale is INT_MIN and the reference map's maximum
    // scale is INT_MIN, don't try to recurse...
    if ((queryNode.Scale() == INT_MIN) &&
       ((*referenceMap.rbegin()).first == INT_MIN))
      break;

    // Get a reference to the current largest scale.
    std::vector<MapEntryType>& scaleVector = (*referenceMap.rbegin()).second;

    // Before traversing all the points in this scale, sort by score.
    std::sort(scaleVector.begin(), scaleVector.end());

    // Now loop over each element.
    for (size_t i = 0; i < scaleVector.size(); ++i)
    {
      // Get a reference to the current element.
      const MapEntryType& frame = scaleVector.at(i);

      CoverTree<MetricType, RootPointPolicy, StatisticType>* refNode =
          frame.referenceNode;
      const double score = frame.score;
      const size_t refIndex = frame.referenceIndex;
      const size_t refPoint = refNode->Point();
      const size_t queryIndex = frame.queryIndex;
      const size_t queryPoint = queryNode.Point();
      double baseCase = frame.baseCase;

//      Log::Debug << "Currently inspecting reference node " << refNode->Point()
//          << " scale " << refNode->Scale() << " earlier query index " <<
//          queryIndex << std::endl;

//      Log::Debug << "Old score " << score << " with refParent " << refIndex
//          << " and queryIndex " << queryIndex << "\n";

      // First we recalculate the score of this node to find if we can prune it.
      if (rule.Rescore(queryNode, *refNode, score) == DBL_MAX)
      {
//        Log::Warn << "Pruned after rescore\n";
        ++numPrunes;
        continue;
      }

      // If this is a self-child, the base case has already been evaluated.
      // We also must ensure that the base case was evaluated with this query
      // point.
      if ((refPoint != refIndex) || (queryPoint != queryIndex))
      {
//        Log::Warn << "Must evaluate base case " << queryNode.Point() << " "
//            << refPoint << "\n";
        baseCase = rule.BaseCase(queryPoint, refPoint);
//        Log::Debug << "Base case " << baseCase << std::endl;
      }

      // Create the score for the children.
      double childScore = rule.Score(queryNode, *refNode, baseCase);

      // Now if this childScore is DBL_MAX we can prune all children.  In this
      // recursion setup pruning is all or nothing for children.
      if (childScore == DBL_MAX)
      {
//        Log::Warn << "Pruned all children.\n";
        numPrunes += refNode->NumChildren();
        continue;
      }

      // We must treat the self-leaf differently.  The base case has already
      // been performed.
      childScore = rule.Score(queryNode, refNode->Child(0), baseCase);

      if (childScore != DBL_MAX)
      {
        MapEntryType newFrame;
        newFrame.referenceNode = &refNode->Child(0);
        newFrame.score = childScore;
        newFrame.baseCase = baseCase;
        newFrame.referenceIndex = refPoint;
        newFrame.queryIndex = queryNode.Point();

        referenceMap[newFrame.referenceNode->Scale()].push_back(newFrame);
      }
      else
      {
        ++numPrunes;
      }

      // Add the non-self-leaf children.
      for (size_t j = 1; j < refNode->NumChildren(); ++j)
      {
        const size_t queryIndex = queryNode.Point();
        const size_t refIndex = refNode->Child(j).Point();

        // We need to incorporate shell() here to try and avoid base case
        // computations.  TODO
//        Log::Debug << "Prescore query " << queryNode.Point() << " scale "
//            << queryNode.Scale() << ", reference " << refNode->Point() <<
//            " scale " << refNode->Scale() << ", reference child " <<
//            refNode->Child(j).Point() << " scale " << refNode->Child(j).Scale()
//            << " with base case " << baseCase;
//        childScore = rule.Prescore(queryNode, *refNode, refNode->Child(j),
//            frame.baseCase);
//        Log::Debug << " and result " << childScore << ".\n";

//        if (childScore == DBL_MAX)
//        {
//          ++numPrunes;
//          continue;
//        }

        // Calculate the base case of each child.
        baseCase = rule.BaseCase(queryIndex, refIndex);

        // See if we can prune it.
        double childScore = rule.Score(queryNode, refNode->Child(j), baseCase);

        if (childScore == DBL_MAX)
        {
          ++numPrunes;
          continue;
        }

        MapEntryType newFrame;
        newFrame.referenceNode = &refNode->Child(j);
        newFrame.score = childScore;
        newFrame.baseCase = baseCase;
        newFrame.referenceIndex = refIndex;
        newFrame.queryIndex = queryIndex;

//        Log::Debug << "Push onto map child " << refNode->Child(j).Point() <<
//            " scale " << refNode->Child(j).Scale() << std::endl;

        referenceMap[newFrame.referenceNode->Scale()].push_back(newFrame);
      }
    }

    // Now clear the memory for this scale; it isn't needed anymore.
    referenceMap.erase((*referenceMap.rbegin()).first);
  }

}

}; // namespace tree
}; // namespace mlpack

#endif

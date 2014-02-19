/**
 * @file range_search_impl.hpp
 * @author Ryan Curtin
 *
 * Implementation of the RangeSearch class.
 */
#ifndef __MLPACK_METHODS_RANGE_SEARCH_RANGE_SEARCH_IMPL_HPP
#define __MLPACK_METHODS_RANGE_SEARCH_RANGE_SEARCH_IMPL_HPP

// Just in case it hasn't been included.
#include "range_search.hpp"

// The rules for traversal.
#include "range_search_rules.hpp"

namespace mlpack {
namespace range {

template<typename MetricType, typename TreeType>
RangeSearch<MetricType, TreeType>::RangeSearch(
    const typename TreeType::Mat& referenceSet,
    const typename TreeType::Mat& querySet,
    const bool naive,
    const bool singleMode,
    const size_t leafSize,
    const MetricType metric) :
    referenceCopy(referenceSet),
    queryCopy(querySet),
    referenceSet(referenceCopy),
    querySet(queryCopy),
    treeOwner(!naive), // If in naive mode, we are not building any trees.
    hasQuerySet(true),
    naive(naive),
    singleMode(!naive && singleMode), // Naive overrides single mode.
    metric(metric),
    numPrunes(0)
{
  // Build the trees.
  Timer::Start("range_search/tree_building");

  // If in naive mode, then we do not need to build trees.
  if (!naive)
  {
    referenceTree = new TreeType(referenceCopy, oldFromNewReferences,
        (naive ? referenceCopy.n_cols : leafSize));

    // If we are in dual-tree mode, we need to build a query tree too.
    if (!singleMode)
      queryTree = new TreeType(queryCopy, oldFromNewQueries,
          (naive ? queryCopy.n_cols : leafSize));
  }

  Timer::Stop("range_search/tree_building");
}

template<typename MetricType, typename TreeType>
RangeSearch<MetricType, TreeType>::RangeSearch(
    const typename TreeType::Mat& referenceSet,
    const bool naive,
    const bool singleMode,
    const size_t leafSize,
    const MetricType metric) :
    referenceCopy(referenceSet),
    referenceSet(referenceCopy),
    querySet(referenceCopy),
    queryTree(NULL),
    treeOwner(!naive), // If in naive mode, we are not building any trees.
    hasQuerySet(false),
    naive(naive),
    singleMode(!naive && singleMode), // Naive overrides single mode.
    metric(metric),
    numPrunes(0)
{
  // Build the trees.
  Timer::Start("range_search/tree_building");

  // If in naive mode, then we do not need to build trees.
  if (!naive)
  {
    referenceTree = new TreeType(referenceCopy, oldFromNewReferences,
        (naive ? referenceCopy.n_cols : leafSize));

    // If using dual-tree mode, then we need a second tree.
    if (!singleMode)
      queryTree = new TreeType(*referenceTree);
  }
  Timer::Stop("range_search/tree_building");
}

template<typename MetricType, typename TreeType>
RangeSearch<MetricType, TreeType>::RangeSearch(
    TreeType* referenceTree,
    TreeType* queryTree,
    const typename TreeType::Mat& referenceSet,
    const typename TreeType::Mat& querySet,
    const bool singleMode,
    const MetricType metric) :
    referenceSet(referenceSet),
    querySet(querySet),
    referenceTree(referenceTree),
    queryTree(queryTree),
    treeOwner(false),
    hasQuerySet(true),
    naive(false),
    singleMode(singleMode),
    metric(metric),
    numPrunes(0)
{
  // Nothing else to initialize.
}

template<typename MetricType, typename TreeType>
RangeSearch<MetricType, TreeType>::RangeSearch(
    TreeType* referenceTree,
    const typename TreeType::Mat& referenceSet,
    const bool singleMode,
    const MetricType metric) :
    referenceSet(referenceSet),
    querySet(referenceSet),
    referenceTree(referenceTree),
    queryTree(NULL),
    treeOwner(false),
    hasQuerySet(false),
    naive(false),
    singleMode(singleMode),
    metric(metric),
    numPrunes(0)
{
  // If doing dual-tree range search, we must clone the reference tree.
  if (!singleMode)
    queryTree = new TreeType(*referenceTree);
}

template<typename MetricType, typename TreeType>
RangeSearch<MetricType, TreeType>::~RangeSearch()
{
  if (treeOwner)
  {
    if (referenceTree)
      delete referenceTree;
    if (queryTree)
      delete queryTree;
  }

  // If doing dual-tree search with one dataset, we cloned the reference tree.
  if (!treeOwner && !hasQuerySet && !(singleMode || naive))
    delete queryTree;
}

template<typename MetricType, typename TreeType>
void RangeSearch<MetricType, TreeType>::Search(
    const math::Range& range,
    std::vector<std::vector<size_t> >& neighbors,
    std::vector<std::vector<double> >& distances)
{
  Timer::Start("range_search/computing_neighbors");

  // Set size of prunes to 0.
  numPrunes = 0;

  // If we have built the trees ourselves, then we will have to map all the
  // indices back to their original indices when this computation is finished.
  // To avoid extra copies, we will store the unmapped neighbors and distances
  // in a separate object.
  std::vector<std::vector<size_t> >* neighborPtr = &neighbors;
  std::vector<std::vector<double> >* distancePtr = &distances;

  if (treeOwner && !(singleMode && hasQuerySet))
    distancePtr = new std::vector<std::vector<double> >;
  if (treeOwner)
    neighborPtr = new std::vector<std::vector<size_t> >;

  // Resize each vector.
  neighborPtr->clear(); // Just in case there was anything in it.
  neighborPtr->resize(querySet.n_cols);
  distancePtr->clear();
  distancePtr->resize(querySet.n_cols);

  // Create the helper object for the traversal.
  typedef RangeSearchRules<MetricType, TreeType> RuleType;
  RuleType rules(referenceSet, querySet, range, *neighborPtr, *distancePtr,
      metric);

  if (naive)
  {
    // The naive brute-force solution.
    for (size_t i = 0; i < querySet.n_cols; ++i)
      for (size_t j = 0; j < referenceSet.n_cols; ++j)
        rules.BaseCase(i, j);
  }
  else if (singleMode)
  {
    // Create the traverser.
    typename TreeType::template SingleTreeTraverser<RuleType> traverser(rules);

    // Now have it traverse for each point.
    for (size_t i = 0; i < querySet.n_cols; ++i)
      traverser.Traverse(i, *referenceTree);

    numPrunes = traverser.NumPrunes();
  }
  else // Dual-tree recursion.
  {
    // Create the traverser.
    typename TreeType::template DualTreeTraverser<RuleType> traverser(rules);

    traverser.Traverse(*queryTree, *referenceTree);

    numPrunes = traverser.NumPrunes();
  }

  Timer::Stop("range_search/computing_neighbors");

  // Output number of prunes.
  Log::Info << "Number of pruned nodes during computation: " << numPrunes
      << "." << std::endl;

  // Map points back to original indices, if necessary.
  if (!treeOwner)
  {
    // No mapping needed.  We are done.
    return;
  }
  else if (treeOwner && hasQuerySet && !singleMode) // Map both sets.
  {
    neighbors.clear();
    neighbors.resize(querySet.n_cols);
    distances.clear();
    distances.resize(querySet.n_cols);

    for (size_t i = 0; i < distances.size(); i++)
    {
      // Map distances (copy a column).
      size_t queryMapping = oldFromNewQueries[i];
      distances[queryMapping] = (*distancePtr)[i];

      // Copy each neighbor individually, because we need to map it.
      neighbors[queryMapping].resize(distances[queryMapping].size());
      for (size_t j = 0; j < distances[queryMapping].size(); j++)
      {
        neighbors[queryMapping][j] = oldFromNewReferences[(*neighborPtr)[i][j]];
      }
    }

    // Finished with temporary objects.
    delete neighborPtr;
    delete distancePtr;
  }
  else if (treeOwner && !hasQuerySet)
  {
    neighbors.clear();
    neighbors.resize(querySet.n_cols);
    distances.clear();
    distances.resize(querySet.n_cols);

    for (size_t i = 0; i < distances.size(); i++)
    {
      // Map distances (copy a column).
      size_t refMapping = oldFromNewReferences[i];
      distances[refMapping] = (*distancePtr)[i];

      // Copy each neighbor individually, because we need to map it.
      neighbors[refMapping].resize(distances[refMapping].size());
      for (size_t j = 0; j < distances[refMapping].size(); j++)
      {
        neighbors[refMapping][j] = oldFromNewReferences[(*neighborPtr)[i][j]];
      }
    }

    // Finished with temporary objects.
    delete neighborPtr;
    delete distancePtr;
  }
  else if (treeOwner && hasQuerySet && singleMode) // Map only references.
  {
    neighbors.clear();
    neighbors.resize(querySet.n_cols);

    // Map indices of neighbors.
    for (size_t i = 0; i < neighbors.size(); i++)
    {
      neighbors[i].resize((*neighborPtr)[i].size());
      for (size_t j = 0; j < neighbors[i].size(); j++)
      {
        neighbors[i][j] = oldFromNewReferences[(*neighborPtr)[i][j]];
      }
    }

    // Finished with temporary object.
    delete neighborPtr;
  }
}

template<typename MetricType, typename TreeType>
std::string RangeSearch<MetricType, TreeType>::ToString() const
{
  std::ostringstream convert;
  convert << "Range Search  [" << this << "]" << std::endl;
  if (treeOwner)
    convert << "  Tree Owner: TRUE" << std::endl;
  if (naive)
    convert << "  Naive: TRUE" << std::endl;
  convert << "  Metric: " << std::endl <<
      mlpack::util::Indent(metric.ToString(),2);
  return convert.str();
}

}; // namespace range
}; // namespace mlpack

#endif

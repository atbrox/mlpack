/** @file tripletree_dfs.h
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef CORE_GNP_TRIPLETREE_DFS_H
#define CORE_GNP_TRIPLETREE_DFS_H

#include "core/metric_kernels/abstract_metric.h"
#include "core/math/range.h"
#include "core/gnp/triple_range_distance_sq.h"

namespace core {
namespace gnp {
template<typename ProblemType>
class TripletreeDfs {

  public:

    typedef typename ProblemType::TableType TableType;
    typedef typename TableType::TreeType TreeType;
    typedef typename ProblemType::GlobalType GlobalType;
    typedef typename ProblemType::ResultType ResultType;

  private:

    ProblemType *problem_;

    TableType *table_;

    int num_deterministic_prunes_;

    int num_monte_carlo_prunes_;

  private:

    void AllocateProbabilities_(
      const std::vector<double> &failure_probabilites,
      const std::deque<bool> &node_is_split,
      const std::deque<bool> &recurse_to_left,
      const std::vector<int> &deterministic_computation_count,
      std::vector<double> *new_failure_probabilities) const;

    bool NodeIsAgreeable_(TreeType *node, TreeType *next_node) const;

    typename TableType::TreeIterator GetNextNodeIterator_(
      const core::gnp::TripleRangeDistanceSq &range_sq_in,
      int node_index,
      const typename TableType::TreeIterator &it_in);

    void ResetStatisticRecursion_(TreeType *node);

    void RecursionHelper_(
      const core::metric_kernels::AbstractMetric &metric,
      core::gnp::TripleRangeDistanceSq &triple_range_distance_sq,
      double relative_error,
      const std::vector<double> &failure_probabilities,
      typename ProblemType::ResultType *query_results,
      int level,
      bool all_leaves,
      std::deque<bool> &node_is_split,
      std::deque<bool> &recurse_to_left,
      std::vector<int> &deterministic_computation_count,
      bool *deterministic_approximation);

    void PreProcess_(TreeType *node);

    void TripletreeBase_(
      const core::metric_kernels::AbstractMetric &metric,
      const core::gnp::TripleRangeDistanceSq &range_in,
      ResultType *result);

    bool CanProbabilisticSummarize_(
      const core::metric_kernels::AbstractMetric &metric,
      const core::gnp::TripleRangeDistanceSq &range_in,
      const std::vector<double> &failure_probabilities,
      int node_start_index,
      typename ProblemType::DeltaType &delta,
      typename ProblemType::ResultType *query_results);

    void ProbabilisticSummarize_(
      const core::metric_kernels::AbstractMetric &metric,
      GlobalType &global,
      const core::gnp::TripleRangeDistanceSq &range_in,
      const std::vector<double> &failure_probabilities,
      int probabilistic_node_start_index,
      const typename ProblemType::DeltaType &delta,
      typename ProblemType::ResultType *query_results);

    bool CanSummarize_(
      const core::gnp::TripleRangeDistanceSq &range_in,
      const typename ProblemType::DeltaType &delta,
      typename ProblemType::ResultType *query_results,
      int *failure_index);

    void Summarize_(
      const core::gnp::TripleRangeDistanceSq &range_in,
      int probabilistic_start_node_index,
      const typename ProblemType::DeltaType &delta,
      typename ProblemType::ResultType *query_results);

    bool TripletreeCanonical_(
      const core::metric_kernels::AbstractMetric &metric,
      core::gnp::TripleRangeDistanceSq &triple_range_distance_sq,
      double relative_error,
      const std::vector<double> &failure_probabilities,
      typename ProblemType::ResultType *query_results);

    void PostProcess_(
      const core::metric_kernels::AbstractMetric &metric,
      TreeType *node, ResultType *query_results,
      bool do_query_results_postprocess);

  public:

    int num_deterministic_prunes() const;

    int num_monte_carlo_prunes() const;

    ProblemType *problem();

    TableType *table();

    void ResetStatistic();

    void Init(ProblemType &problem_in);

    void Compute(
      const core::metric_kernels::AbstractMetric &metric,
      typename ProblemType::ResultType *query_results);
};
};
};

#endif

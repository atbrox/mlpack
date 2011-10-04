/** @file query_subtable_lock.h
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef CORE_PARALLEL_QUERY_SUBTABLE_LOCK_H
#define CORE_PARALLEL_QUERY_SUBTABLE_LOCK_H

#include <boost/intrusive_ptr.hpp>
#include <deque>
#include <list>
#include <omp.h>
#include <vector>
#include "core/math/range.h"
#include "core/parallel/disjoint_int_intervals.h"

namespace core {
namespace parallel {

template < typename DistributedTableType,
         typename TaskPriorityQueueType >
class DistributedDualtreeTaskQueue;

/** @brief The lock on a query subtable.
 */
template < typename DistributedTableType,
         typename TaskPriorityQueueType >
class QuerySubTableLock {
  private:

    /** @brief The type of the task associated with the priority queue
     *         associated with the query subtable.
     */
    typedef typename TaskPriorityQueueType::value_type TaskType;

    /** @brief The type of the distributed task queue.
     */
    typedef DistributedDualtreeTaskQueue <
    DistributedTableType,
    TaskPriorityQueueType > DistributedDualtreeTaskQueueType;

    /** @brief The friend declaration with the distributed task queue.
     */
    friend class core::parallel::DistributedDualtreeTaskQueue <
      DistributedTableType,
        TaskPriorityQueueType >;

    /** @brief The table type used in the exchange process.
     */
    typedef typename DistributedTableType::TableType TableType;

    /** @brief The subtable type used in the exchange process.
     */
    typedef core::table::SubTable<TableType> SubTableType;

    /** @brief The ID of subtables.
     */
    typedef typename SubTableType::SubTableIDType SubTableIDType;

    /** @brief The type of the lock on query subtable.
     */
    typedef QuerySubTableLock <
    DistributedTableType, TaskPriorityQueueType > QuerySubTableLockType;

    /** @brief The MPI rank of the process holding the query subtable.
     */
    int locked_mpi_rank_;

    /** @brief The query subtable.
     */
    boost::intrusive_ptr< SubTableType > query_subtable_;

    /** @brief The remaining work for query subtable.
     */
    unsigned long int remaining_work_for_query_subtable_;

    /** @brief The priority queue associated with the tasks that are
     *         alive for the query subtable.
     */
    boost::intrusive_ptr<TaskPriorityQueueType> task_;

    /** @brief The total number of remaining tasks on this MPI process
     *         (pointer reference).
     */
    int *num_remaining_tasks_;

    /** @brief The total quantity of local computation remaining on
     *         this MPI process (pointer reference).
     */
    unsigned long int *remaining_local_computation_;

  private:

    void CheckOut_(
      DistributedDualtreeTaskQueueType *checkout_from, int probe_index,
      int locked_mpi_rank_in) {

      // Set the locked MPI rank.
      locked_mpi_rank_ = locked_mpi_rank_in;

      // Set the variables for keeping track of the remaining
      // computations.
      num_remaining_tasks_ = & (checkout_from->num_remaining_tasks_);
      remaining_local_computation_ =
        & (checkout_from->remaining_local_computation_);

      // Check out from the position.
      query_subtable_ = checkout_from->query_subtables_[probe_index];
      remaining_work_for_query_subtable_ =
        checkout_from->remaining_work_for_query_subtables_[probe_index];
      task_ = checkout_from->tasks_[probe_index];

      // Overwrite the current position with the back item.
      checkout_from->query_subtables_[probe_index] =
        checkout_from->query_subtables_.back();
      checkout_from->remaining_work_for_query_subtables_[probe_index] =
        checkout_from->remaining_work_for_query_subtables_.back();
      checkout_from->tasks_[probe_index] = checkout_from->tasks_.back();

      // Pop the back items.
      checkout_from->query_subtables_.pop_back();
      checkout_from->remaining_work_for_query_subtables_.pop_back();
      checkout_from->tasks_.pop_back();
    }

    void Return_(DistributedDualtreeTaskQueueType *export_to) {
      export_to->query_subtables_.push_back(query_subtable_);
      export_to->remaining_work_for_query_subtables_.push_back(
        remaining_work_for_query_subtable_);
      export_to->tasks_.push_back(task_);

      // Put it back in the front again.
      if(export_to->tasks_.size() > 1) {
        export_to->query_subtables_.back().swap(
          export_to->query_subtables_.front());
        std::swap(
          export_to->remaining_work_for_query_subtables_.back(),
          export_to->remaining_work_for_query_subtables_.front());
        export_to->tasks_.back().swap(export_to->tasks_.front());
      }
    }

    template<typename MetricType>
    void PushTask_(
      DistributedDualtreeTaskQueueType *queue_in,
      boost::mpi::communicator &world, const MetricType &metric_in,
      SubTableType &reference_subtable) {

      // Compute the priority and push in.
      core::math::Range squared_distance_range(
        query_subtable_->start_node()->bound().RangeDistanceSq(
          metric_in, reference_subtable.start_node()->bound()));
      double priority = - squared_distance_range.mid() -
                        (queue_in->process_rank_favor_factor_) *
                        (queue_in->table_exchange_).process_rank(
                          world, reference_subtable.table()->rank());
      TaskType new_task(
        *(query_subtable_), reference_subtable, priority);
      task_->push(new_task);

      // Increment the number of tasks.
      (* num_remaining_tasks_)++;

      // Increment the available local computation.
      (* remaining_local_computation_) += new_task.work();
    }

  public:

    long reference_count_;

  public:

    SubTableIDType subtable_id() const {
      return query_subtable_->subtable_id();
    }

    QuerySubTableLock() {
      locked_mpi_rank_ = -1;
      num_remaining_tasks_ = NULL;
      reference_count_ = 0;
      remaining_local_computation_ = NULL;
      remaining_work_for_query_subtable_ = 0;
    }

    void operator=(const QuerySubTableLockType &lock_in) {
      num_remaining_tasks_ = lock_in.num_remaining_tasks_;
      query_subtable_  = lock_in.query_subtable_;
      remaining_local_computation_ = lock_in.remaining_local_computation_;
      remaining_work_for_query_subtable_ =
        lock_in.remaining_work_for_query_subtable_;
      task_ = lock_in.task_;
    }

    QuerySubTableLock(const QuerySubTableLockType &lock_in) {
      reference_count_ = 0;
      this->operator=(lock_in);
    }
};

template < typename DistributedTableType,
         typename TaskPriorityQueueType >
inline void intrusive_ptr_add_ref(
  QuerySubTableLock<DistributedTableType, TaskPriorityQueueType> *ptr) {
  ptr->reference_count_++;
}

template < typename DistributedTableType,
         typename TaskPriorityQueueType >
inline void intrusive_ptr_release(
  QuerySubTableLock<DistributedTableType, TaskPriorityQueueType> *ptr) {
  ptr->reference_count_--;
  if(ptr->reference_count_ == 0) {
    if(core::table::global_m_file_) {
      core::table::global_m_file_->DestroyPtr(ptr);
    }
    else {
      delete ptr;
    }
  }
}
}
}

#endif

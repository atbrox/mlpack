/** @file distributed_table_test.cc
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */
#include "core/metric_kernels/lmetric.h"
#include "core/table/distributed_table.h"
#include "core/table/mailbox.h"
#include "core/tree/gen_kdtree.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

typedef core::tree::GeneralBinarySpaceTree < core::tree::GenKdTree > TreeType;
typedef core::table::Table<TreeType> TableType;

bool CheckDistributedTableIntegrity(
  const core::table::DistributedTable &table_in,
  const boost::mpi::communicator &world,
  const boost::mpi::communicator &table_outbox_group,
  const boost::mpi::communicator &table_inbox_group) {
  for(int i = 0; i < world.size(); i++) {
    printf(
      "Process %d thinks Process %d owns %d points of dimensionality %d.\n",
      world.rank(), i, table_in.local_n_entries(i % (world.size() / 3)),
      table_in.n_attributes());
  }
  return true;
}

core::table::DistributedTable *InitDistributedTable(
  boost::mpi::communicator &world,
  boost::mpi::communicator &table_outbox_group) {

  std::pair< core::table::DistributedTable *, std::size_t >
  distributed_table_pair =
    core::table::global_m_file_->UniqueFind<core::table::DistributedTable>();
  core::table::DistributedTable *distributed_table =
    distributed_table_pair.first;

  if(distributed_table == NULL) {
    printf("Process %d: TableOutbox.\n", world.rank());

    // Each process generates its own random data, dumps it to the file,
    // and read its own file back into its own distributed table.
    core::table::Table<TreeType> random_dataset;
    const int num_dimensions = 5;
    int num_points = core::math::RandInt(10, 20);
    random_dataset.Init(5, num_points);
    for(int j = 0; j < num_points; j++) {
      core::table::DensePoint point;
      random_dataset.get(j, &point);
      for(int i = 0; i < num_dimensions; i++) {
        point[i] = core::math::Random(0.1, 1.0);
      }
    }
    printf("Process %d generated %d points...\n", world.rank(), num_points);
    std::stringstream file_name_sstr;
    file_name_sstr << "random_dataset_" << world.rank() << ".csv";
    std::string file_name = file_name_sstr.str();
    random_dataset.Save(file_name);

    std::stringstream distributed_table_name_sstr;
    distributed_table_name_sstr << "distributed_table_" << world.rank() << "\n";
    distributed_table = core::table::global_m_file_->UniqueConstruct <
                        core::table::DistributedTable > ();
    distributed_table->Init(
      file_name, table_outbox_group);
    printf(
      "Process %d read in %d points...\n",
      world.rank(), distributed_table->local_n_entries());
  }
  return distributed_table;
}

void TableOutboxProcess(
  core::table::DistributedTable *distributed_table,
  boost::mpi::communicator &world,
  boost::mpi::communicator &table_outbox_group,
  boost::mpi::communicator &table_inbox_group,
  boost::mpi::communicator &computation_group) {

  printf("Process %d: TableOutbox.\n", world.rank());
  distributed_table->RunOutbox(
    table_outbox_group, table_inbox_group, computation_group);
}

void TableInboxProcess(
  core::table::DistributedTable *distributed_table,
  boost::mpi::communicator &world,
  boost::mpi::communicator &table_outbox_group,
  boost::mpi::communicator &table_inbox_group,
  boost::mpi::communicator &computation_group) {
  printf("Process %d: TableInbox.\n", world.rank());

  distributed_table->RunInbox(
    table_outbox_group, table_inbox_group, computation_group);
}

void ComputationProcess(
  core::table::DistributedTable *distributed_table,
  boost::mpi::communicator &world,
  boost::mpi::communicator &table_outbox_group,
  boost::mpi::communicator &table_inbox_group,
  boost::mpi::communicator &computation_group) {

  printf("Process %d: Computation.\n", world.rank());

  // Do a test where each computation process requests a random point
  // from a randomly chosen process.
  int num_points = core::math::RandInt(10, 30);
  for(int n = 0; n < num_points; n++) {
    core::table::DenseConstPoint point;
    int random_request_rank = core::math::RandInt(0, table_outbox_group.size());
    int random_request_point_id =
      core::math::RandInt(
        0, distributed_table->local_n_entries(random_request_rank));
    distributed_table->get(
      table_outbox_group, table_inbox_group,
      random_request_rank, random_request_point_id, &point);

    // Print the point.
    point.Print();

    // Tell the inbox that we are done using the point.
    distributed_table->UnlockPointinTableInbox();
  }
}

int main(int argc, char *argv[]) {

  // Initialize boost MPI.
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  if(world.size() <= 1 || world.size() % 3 != 0) {
    std::cout << "Please specify a process number greater than 1 and "
              "a multiple of 3.\n";
    return 0;
  }

  // Delete the teporary files and put a barrier.
  std::stringstream temporary_file_name;
  temporary_file_name << "tmp_file" << world.rank();
  remove(temporary_file_name.str().c_str());
  world.barrier();

  // Initialize the memory allocator.
  core::table::global_m_file_ = new core::table::MemoryMappedFile();
  core::table::global_m_file_->Init(
    std::string("tmp_file"), world.rank(),
    world.rank() % (world.size() / 3), 5000000);

  // Seed the random number.
  srand(time(NULL) + world.rank());

  if(world.rank() == 0) {
    printf("%d processes are present...\n", world.size());
  }

  // If the process ID is less than half of the size of the
  // communicator, make it a table process. Otherwise, make it a
  // computation process. This assignment depends heavily on the
  // round-robin assignment of mpirun.
  boost::mpi::group world_group = world.group();
  std::vector<int> table_outbox_group_vector(world.size() / 3, 0);
  std::vector<int> table_inbox_group_vector(world.size() / 3, 0);
  std::vector<int> computation_group_vector(world.size() / 3, 0);
  for(int i = 0; i < world.size() / 3; i++) {
    table_outbox_group_vector[i] = i;
    table_inbox_group_vector[i] = i + world.size() / 3;
    computation_group_vector[i] = i + world.size() / 3 * 2;
  }

  boost::mpi::group table_outbox_group =
    world_group.include(
      table_outbox_group_vector.begin(), table_outbox_group_vector.end());
  boost::mpi::communicator table_outbox_group_comm(world, table_outbox_group);
  boost::mpi::group table_inbox_group =
    world_group.include(
      table_inbox_group_vector.begin(), table_inbox_group_vector.end());
  boost::mpi::communicator table_inbox_group_comm(world, table_inbox_group);
  boost::mpi::group computation_group =
    world_group.include(
      computation_group_vector.begin(), computation_group_vector.end());
  boost::mpi::communicator computation_group_comm(world, computation_group);

  // Create the intercommunicator between the current process and each
  // of the subgroups.
  //  if(world.rank() >= world.size() / 3) {
  table_outbox_group_vector.push_back(world.rank());
  //}
  //if( world.rank() < world.size() / 3 ||
  //     world.rank() >= world.size() / 3 * 2 ) {
  table_inbox_group_vector.push_back(world.rank());
  //}
  //if( world.rank() < world.size() / 3 * 2) {
  computation_group_vector.push_back(world.rank());
  // }
  boost::mpi::group table_outbox_inter_group =
    world_group.include(
      table_outbox_group_vector.begin(), table_outbox_group_vector.end());
  boost::mpi::communicator table_outbox_group_inter_comm(
    world, table_outbox_inter_group);
  boost::mpi::group table_inbox_inter_group =
    world_group.include(
      table_inbox_group_vector.begin(), table_inbox_group_vector.end());
  boost::mpi::communicator table_inbox_group_inter_comm(
    world, table_inbox_inter_group);
  boost::mpi::group computation_inter_group =
    world_group.include(
      computation_group_vector.begin(), computation_group_vector.end());
  boost::mpi::communicator computation_group_inter_comm(
    world, computation_inter_group);

  // Declare the distributed table.
  core::table::DistributedTable *distributed_table = NULL;

  // Wait until the memory allocator is in synch.
  world.barrier();

  // Read the distributed table once per each compute node, and put a
  // barrier.
  if(world.rank() < world.size() / 3) {
    distributed_table =
      InitDistributedTable(world, table_outbox_group_comm);
  }
  world.barrier();

  // Attach the distributed table for all the processes and put a
  // barrier.
  std::pair< core::table::DistributedTable *, std::size_t >
  distributed_table_pair =
    core::table::global_m_file_->UniqueFind<core::table::DistributedTable>();
  distributed_table = distributed_table_pair.first;

  // Check the integrity of the distributed table.
  CheckDistributedTableIntegrity(
    *distributed_table, world,
    table_outbox_group_inter_comm, table_inbox_group_inter_comm);

  // The main computation loop.
  if(world.rank() < world.size() / 3) {
    TableOutboxProcess(
      distributed_table, world, table_outbox_group_inter_comm,
      table_inbox_group_inter_comm, computation_group_inter_comm);
  }
  else if(world.rank() < world.size() / 3 * 2) {
    TableInboxProcess(
      distributed_table, world, table_outbox_group_inter_comm,
      table_inbox_group_inter_comm, computation_group_inter_comm);
  }
  else {
    ComputationProcess(
      distributed_table, world,
      table_outbox_group_inter_comm, table_inbox_group_inter_comm,
      computation_group_inter_comm);
  }

  return 0;
}

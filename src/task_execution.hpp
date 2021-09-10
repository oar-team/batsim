#pragma once

#include "context.hpp"
#include "ipp.hpp"
#include "jobs.hpp"

int execute_parallel_task(
    BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & allocation,
    double * remaining_time,
    BatsimContext * context
);

int execute_smpi_trace_replay(
    BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & allocation,
    double * remaining_time,
    BatsimContext * context
);

void prepare_ptask(
    const BatsimContext * context,
    const BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement,
    std::vector<double> & computation_vector,
    std::vector<double> & communication_matrix,
    std::vector<simgrid::s4u::Host *> & hosts_to_use,
    std::vector<Machine *> & machines_to_use
);

void hosts_from_predefined_strategy(
    const BatsimContext * context,
    int nb_executors,
    const IntervalSet & allocated_hosts,
    batprotocol::fb::PredefinedExecutorPlacementStrategy predefined_strategy,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    std::vector<Machine*> & machines_to_use
);

void hosts_from_custom_executor_to_host_mapping(
    const BatsimContext * context,
    int nb_executors,
    const IntervalSet & allocated_hosts,
    const std::vector<unsigned int> & custom_mapping,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    std::vector<Machine*> & machines_to_use
);

void hosts_from_alloc_placement(
    const BatsimContext * context,
    int nb_executors,
    const std::shared_ptr<AllocationPlacement> & allocation,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    std::vector<Machine*> & machines_to_use
);

int determine_task_nb_executors(
    const BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement
);

const Machine * machine_from_storage_label(
    const std::string & storage_label,
    const std::map<std::string, int> & storage_mapping,
    const BatsimContext * context
);

#pragma once

#include "context.hpp"
#include "ipp.hpp"
#include "jobs.hpp"

/**
 * @brief Execute a task that corresponds to a parallel task profile
 * @param[in,out] btask The task to execute. Progress information is stored within it.
 * @param[in] allocation Where the task should be executed
 * @param[in,out] remaining_time The remaining time of the current task. It will be automatically killed if 0 is reached.
 * @param[in] context The BatsimContext
 * @return The profile return code on success, -1 on timeout (remaining time reached 0), -2 on task cancelled (issued by another SimGrid actor)
 */
int execute_parallel_task(
    BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & allocation,
    double * remaining_time,
    BatsimContext * context
);

/**
 * @brief Execute a task that corresponds to a trace replay profile
 * @param[in,out] btask The task to execute. Progress information is stored within it.
 * @param[in] allocation Where the task should be executed
 * @param[in,out] remaining_time The remaining time of the current task. It will be automatically killed if 0 is reached.
 * @param[in] context The BatsimContext
 * @return The profile return code on success, -1 on timeout (remaining time reached 0), -2 on task cancelled (issued by another SimGrid actor)
 */
int execute_smpi_trace_replay(
    BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & allocation,
    double * remaining_time,
    BatsimContext * context
);

/**
 * @brief Prepare all data required to execute a ptask, in particular computation/communication matrices and hosts to use
 * @param[in] context The BatsimContext
 * @param[in] btask The task to execute
 * @param[in] alloc_placement The allocation/placement to use for the ptask
 * @param[out] computation_vector The computation vector of the ptask
 * @param[out] communication_matrix The communication matrix of the ptask
 * @param[out] hosts_to_use The hosts that will be used to execute the ptask
 */
void prepare_ptask(
    const BatsimContext * context,
    const BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement,
    std::vector<double> & computation_vector,
    std::vector<double> & communication_matrix,
    std::vector<simgrid::s4u::Host *> & hosts_to_use,
    std::vector<Machine *> & machines_to_use
);

/**
 * @brief Generate a host list from a set of hosts and a predefined strategy
 * @param[in] context The BatsimContext
 * @param[in] nb_executors The number of executors (=SimGrid actors) that will execute this profile
 * @param[in] allocated_hosts The allocated hosts to use
 * @param[in] predefined_strategy The predefined executor placement strategy to apply
 * @param[out] hosts_to_use The resulting host list
 * @param[out] machines_to_use The resulting machine list
 */
void hosts_from_predefined_strategy(
    const BatsimContext * context,
    int nb_executors,
    const IntervalSet & allocated_hosts,
    batprotocol::fb::PredefinedExecutorPlacementStrategy predefined_strategy,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    std::vector<Machine*> & machines_to_use
);

/**
 * @brief Generate a host list from a set of hosts and a custom executor to host mapping.
 * @param[in] context The BatsimContext
 * @param[in] nb_executors The number of executors (=SimGrid actors) that will execute this profile
 * @param[in] allocated_hosts The allocated hosts to use
 * @param[in] custom_mapping The custom executor to host mapping. Values are expected to be in [0, allocated_hosts.size()[.
 * @param[out] hosts_to_use The resulting host list
 * @param[out] machines_to_use The resulting machine list
 */
void hosts_from_custom_executor_to_host_mapping(
    const BatsimContext * context,
    int nb_executors,
    const IntervalSet & allocated_hosts,
    const std::vector<unsigned int> & custom_mapping,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    std::vector<Machine*> & machines_to_use
);

/**
 * @brief Generate a host list from an AllocationPlacement.
 * @param[in] context The BatsimContext
 * @param[in] nb_executors The number of executors (=SimGrid actors) that will execute this profile
 * @param[in] allocation The AllocationPlacement
 * @param[out] hosts_to_use The resulting host list
 * @param[out] machines_to_use The resulting machine list
 */
void hosts_from_alloc_placement(
    const BatsimContext * context,
    int nb_executors,
    const std::shared_ptr<AllocationPlacement> & allocation,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    std::vector<Machine*> & machines_to_use
);

/**
 * @brief Determine how many executors (=SimGrid actors) should execute a given task
 * @param[in] btask The BatTask to execute
 * @param[in] alloc_placement The allocation/placement of the profile
 * @return The number of executors to execute the profile
 */
int determine_task_nb_executors(
    const BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement
);

/**
 * @brief Returns a storage machine from a storage label
 * @param[in] storage_label The machine to search by name
 * @param[in] storage_mapping Mapping from storage labels to machine ids
 * @param[in] context The BatsimContext
 * @return
 */
const Machine * machine_from_storage_label(
    const std::string & storage_label,
    const std::map<std::string, int> & storage_mapping,
    const BatsimContext * context
);

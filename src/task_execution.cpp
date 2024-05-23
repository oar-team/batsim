/**
 * @file task_execution.cpp
 * @brief Contains functions related to the execution of the parallel profile tasks
 */

#include "task_execution.hpp"

#include <unordered_set>

#include <simgrid/s4u.hpp>
#include <smpi/smpi.h>

#include "jobs.hpp"
#include "profiles.hpp"
#include "ipp.hpp"
#include "context.hpp"
#include "jobs_execution.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(task_execution, "task_execution"); //!< Logging

using namespace std;
using namespace roles;

/**
 * @brief Generate the communication and computaion matrix for the
 *        parallel task profile. Also set the prefix name of the task.
 * @param[out] computation_amount the computation matrix to be simulated by the parallel task
 * @param[out] communication_amount the communication matrix to be simulated by the parallel task
 * @param[in] nb_executors the number of resources the task have to run on
 * @param[in] profile_data the profile data
 */
void generate_parallel_task(
    std::vector<double> & computation_amount,
    std::vector<double> & communication_amount,
    unsigned int nb_executors,
    void * profile_data)
{
    auto * data = static_cast<ParallelProfileData*>(profile_data);
    xbt_assert(nb_executors == data->nb_res,
            "the number of executors (%u) is different than the rigid parallel task size given in the profile (%d)",
            nb_executors, data->nb_res);

    // Prepare buffers
    computation_amount.resize(nb_executors, 0);
    communication_amount.resize(nb_executors*nb_executors, 0);

    // Retrieve the matrices from the profile
    memcpy(computation_amount.data(), data->cpu, sizeof(double) * nb_executors);
    memcpy(communication_amount.data(), data->com, sizeof(double) * nb_executors * nb_executors);
}

/**
 * @brief Generate the communication and computaion matrix for the
 *        parallel homogeneous task profile. Also set the prefix name of the task.
 * @param[out] computation_amount the computation matrix to be simulated by the parallel task
 * @param[out] communication_amount the communication matrix to be simulated by the parallel task
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 */
void generate_parallel_task_homogeneous(
    std::vector<double> & computation_amount,
    std::vector<double> & communication_amount,
    unsigned int nb_res,
    void * profile_data)
{
    auto * data = static_cast<ParallelHomogeneousProfileData*>(profile_data);

    // Determine how much computation/communication must be put into each matrix value.
    double cpu = data->cpu;
    double com = data->com;

    if (data->strategy == batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsSpreadUniformly)
    {
        // User-given value is just fair shared on all executors.
        cpu = data->cpu / nb_res;

        // Values in the diagonal are zeros. There should be n² - n non-zero values, which get a fair share of the user-defined communication amount.
        const int nb_non_zero_transfers = nb_res * nb_res - nb_res;
        com = data->com / nb_non_zero_transfers;
    }

    // Generate a computation vector. Either empty if no computation is requested, or of size nb_res with all identical values otherwise.
    if (cpu <= 0)
        computation_amount.clear();
    else
        computation_amount = std::vector<double>(nb_res, cpu);

    // Generate a communication matrix. Empty if no communication requested, nb_res*nb_res matrix with 0 in diagonal otherwise.
    if (com <= 0)
        communication_amount.clear();
    else
    {
        // Completely fill the matrix with 'com' values.
        communication_amount = std::vector<double>(nb_res*nb_res, com);

        // Erase values in the diagonal with 0.
        for (unsigned int i = 0; i < nb_res; ++i)
        {
            communication_amount[i*nb_res+i] = 0.0;
        }
    }
}

/**
 * @brief Generate the communication and computaion matrix for the
 *        parallel homogeneous task profile with a Parallel File System.
 *
 * @param[out] computation_amount the computation matrix to be simulated by the parallel task
 * @param[out] communication_amount the communication matrix to be simulated by the parallel task
 * @param[in,out] hosts_to_use the list of host to be used by the task
 * @param[in] storage_mapping mapping from label given in the profile and machine id
 * @param[in] profile_data the profile data
 * @param[in] context the batsim context
 *
 * @details Note that the number of resource is also altered because of the
 *          pfs node that is addded.
 */
void generate_parallel_task_on_storage_homogeneous(
    std::vector<double> & computation_amount,
    std::vector<double> & communication_amount,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    const std::map<std::string, int> & storage_mapping,
    void * profile_data,
    const BatsimContext * context)
{
    auto * data = static_cast<ParallelTaskOnStorageHomogeneousProfileData*>(profile_data);

    // Another host must be added to the lists of hosts to use (the storage to use).
    auto * storage_machine = machine_from_storage_label(data->storage_label, storage_mapping, context);

    // Determine how much data should be read/written.
    double bytes_to_read = data->bytes_to_read;
    double bytes_to_write = data->bytes_to_read;

    if (data->strategy == batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsSpreadUniformly)
    {
        // User-given values are fair-shared between the hosts already in the list.
        bytes_to_read = data->bytes_to_read / hosts_to_use.size();
        bytes_to_write = data->bytes_to_write / hosts_to_use.size();
    }

    const int storage_index = hosts_to_use.size(); // The index in hosts_to_use where the storage will be.
    hosts_to_use.push_back(storage_machine->host);
    const int nb_executors = hosts_to_use.size();

    // This profile does not do any computation.
    computation_amount.clear();

    // Generate a communication matrix if needed.
    bool do_comm = bytes_to_read > 0 || bytes_to_write > 0;
    if (!do_comm)
    {
        communication_amount.clear();
    }
    else
    {
        // Most of the matrix is filled with zeros, as allocated hosts never communicate with each other with this profile. The storage is always at the end of the host list.
        /* Communication matrix example for 4 allocated hosts:
         * 0 0 0 0 w
         * 0 0 0 0 w
         * 0 0 0 0 w
         * 0 0 0 0 w
         * r r r r 0
         */
        communication_amount = std::vector<double>(nb_executors*nb_executors, 0.0);
        for (int host_index = 0; host_index < storage_index; ++host_index)
        {
            // The final row contains bytes_to_read, as the storage host will send this data to all other hosts.
            communication_amount[nb_executors*storage_index+host_index] = bytes_to_read;

            // The final column contains bytes_to_write, as the storage host will receive this data from all other hosts.
            communication_amount[nb_executors*host_index+storage_index] = bytes_to_write;
        }
    }
}

/**
 * @brief Generate the communication and computaion matrix for the
 *        data staging task profile.
 * @details Note that the number of resource is also altered because only
 * the pfs and the hpst are involved in the transfer. It also set the prefix
 * name of the task.
 * @param[out] computation_amount the computation matrix to be simulated by the parallel task
 * @param[out] communication_amount the communication matrix to be simulated by the parallel task
 * @param[in,out] hosts_to_use the list of host to be used by the task
 * @param[in] storage_mapping mapping from label given in the profile and machine id
 * @param[in] profile_data the profile data
 * @param[in] context the batsim context
 */
void generate_parallel_task_data_staging_between_storages(
    std::vector<double> & computation_amount,
    std::vector<double> & communication_amount,
    std::vector<simgrid::s4u::Host*> & hosts_to_use,
    const std::map<std::string, int> & storage_mapping,
    void * profile_data,
    const BatsimContext * context)
{
    auto * data = static_cast<DataStagingProfileData*>(profile_data);

    // Find the storages to use.
    auto * emitter_storage_machine = machine_from_storage_label(data->from_storage_label, storage_mapping, context);
    auto * receiver_storage_machine = machine_from_storage_label(data->to_storage_label, storage_mapping, context);

    // Override the hosts to use. Only the storages will be used for this profile.
    hosts_to_use = std::vector<simgrid::s4u::Host*>({emitter_storage_machine->host, receiver_storage_machine->host});
    const int nb_executors = 2;

    // No computation to do here.
    computation_amount.clear();

    // Generate a communication matrix if needed.
    if (data->nb_bytes <= 0)
    {
        communication_amount.clear();
    }
    else
    {
        // Matrix has this shape: 0.0 everywhere but on the value that let the emitter sends to the receiver.
        /* 0 b
         * 0 0
         */
        communication_amount = std::vector<double>(nb_executors, 0.0);
        communication_amount[1] = data->nb_bytes;
    }
}

/**
 * @brief Debug print of a parallel task (via XBT_DEBUG)
 * @param[in] computation_vector The ptask computation vector
 * @param[in] communication_matrix The ptask communication matrix
 * @param[in] nb_res The number of hosts involved in the parallel task
 * @param[in] alloc The resource ids allocated for the parallel task
 * @param[in] mapping The mapping between executor id and resource id, if any
 */
void debug_print_ptask(const std::vector<double>& computation_vector,
                       const std::vector<double>& communication_matrix,
                       unsigned int nb_res,
                       const IntervalSet alloc,
                       const vector<int> mapping = vector<int>())
{
    string comp = "";
    string comm = "";
    int k = 0;
    for (unsigned int i=0; i < nb_res; i++)
    {
        if (!computation_vector.empty())
        {
            int alloc_i = mapping.empty() ? alloc[i] : alloc[mapping[i]];
            comp += to_string(alloc_i) + ": " + to_string(computation_vector[i]) + ", ";
        }
        if (!communication_matrix.empty())
        {
            for (unsigned int j = 0; j < nb_res; j++)
            {
                int alloc_i = mapping.empty() ? alloc[i] : alloc[mapping[i]];
                int alloc_j = mapping.empty() ? alloc[j] : alloc[mapping[j]];
                comm += to_string(alloc_i) + "->" + to_string(alloc_j) + ": " + to_string(communication_matrix[k++]) + ", ";
            }
            comm += "\n";
        }
    }

    XBT_DEBUG("Generated matrices: \nCompute: \n%s\nComm:\n%s", comp.c_str(), comm.c_str());
}

/**
 * @brief Execute a task that corresponds to a parallel task profile
 * @param[in,out] btask The task to execute. Progress information is stored within it.
 * @param[in] alloc_placement Where the task should be executed
 * @param[in,out] remaining_time The remaining time of the current task. It will be automatically killed if 0 is reached.
 * @param[in] context The BatsimContext
 * @return The profile return code on success, -1 on timeout (remaining time reached 0), -2 on task cancelled (issued by another SimGrid actor)
 */
int execute_parallel_task(
    BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement,
    double * remaining_time,
    BatsimContext * context)
{
    auto profile = btask->profile;

    std::vector<simgrid::s4u::Host*> hosts_to_use;
    std::vector<Machine *> machines_to_use;
    std::vector<double> computation_vector;
    std::vector<double> communication_matrix;
    prepare_ptask(context, btask, alloc_placement, computation_vector, communication_matrix, hosts_to_use, machines_to_use);

    // Create the parallel task
    string task_name = profile_type_to_string(profile->type) + '_' + static_cast<JobPtr>(btask->parent_job)->id.to_string() +
                       "_" + btask->profile->name;
    XBT_DEBUG("Creating parallel task '%s' on %zu resources", task_name.c_str(), hosts_to_use.size());

    simgrid::s4u::ExecPtr ptask = simgrid::s4u::this_actor::exec_init(hosts_to_use, computation_vector, communication_matrix);
    ptask->set_name(task_name.c_str());

    // Keep track of the task to get information on kill
    btask->ptask = ptask;

    // Execute the parallel task
    int ret = profile->return_code;
    double time_start = simgrid::s4u::Engine::get_clock();
    if (*remaining_time < 0)
    {
        XBT_DEBUG("Executing task '%s' without walltime", task_name.c_str());
        try
        {
            ptask->start();
            ptask->wait();
        }
        catch (const simgrid::CancelException &)
        {
            XBT_DEBUG("Task '%s' has been cancelled (a kill was asked).", task_name.c_str());
            ret = -2;
        }
    }
    else
    {
        double time_before_execute = simgrid::s4u::Engine::get_clock();
        XBT_DEBUG("Executing task '%s' with walltime of %g", task_name.c_str(), *remaining_time);
        try
        {
            ptask->start();
            ptask->wait_for(*remaining_time);
        }
        catch (const simgrid::TimeoutException &)
        {
            // The ptask reached the walltime
            XBT_DEBUG("Task '%s' reached its walltime.", task_name.c_str());
            ret = -1;
        }
        catch (const simgrid::CancelException &)
        {
            XBT_DEBUG("Task '%s' has been cancelled (a kill was asked).", task_name.c_str());
            ret = -2;
        }
        *remaining_time = *remaining_time - (simgrid::s4u::Engine::get_clock() - time_before_execute);
    }

    XBT_DEBUG("Task '%s' finished in %f", task_name.c_str(),
        simgrid::s4u::Engine::get_clock() - time_start);

    return ret;
}

/**
 * @brief Prepare all data required to execute a ptask, in particular computation/communication matrices and hosts to use
 * @param[in] context The BatsimContext
 * @param[in] btask The task to execute
 * @param[in] alloc_placement The allocation/placement to use for the ptask
 * @param[out] computation_vector The computation vector of the ptask
 * @param[out] communication_matrix The communication matrix of the ptask
 * @param[out] hosts_to_use The hosts that will be used to execute the ptask
 * @param[out] machines_to_use The hosts that will be used to execute the ptask
 */
void prepare_ptask(
    const BatsimContext * context,
    const BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement,
    std::vector<double> & computation_vector,
    std::vector<double> & communication_matrix,
    std::vector<simgrid::s4u::Host *> & hosts_to_use,
    std::vector<Machine *> & machines_to_use)
{
    int nb_executors = determine_task_nb_executors(btask, alloc_placement);
    hosts_from_alloc_placement(context, nb_executors, alloc_placement, hosts_to_use, machines_to_use);

    switch(btask->profile->type)
    {
    case ProfileType::PTASK: {
        generate_parallel_task(
            computation_vector,
            communication_matrix,
            nb_executors,
            btask->profile->data);
    } break;
    case ProfileType::PTASK_HOMOGENEOUS: {
        generate_parallel_task_homogeneous(
            computation_vector,
            communication_matrix,
            nb_executors,
            btask->profile->data);
    } break;
    case ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS: {
        generate_parallel_task_on_storage_homogeneous(
            computation_vector,
            communication_matrix,
            hosts_to_use,
            static_cast<JobPtr>(btask->parent_job)->execution_request->storage_mapping,
            btask->profile->data,
            context);
    } break;
    case ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES: {
        generate_parallel_task_data_staging_between_storages(
            computation_vector,
            communication_matrix,
            hosts_to_use,
            static_cast<JobPtr>(btask->parent_job)->execution_request->storage_mapping,
            btask->profile->data,
            context);
    } break;
    default: {
        xbt_die("Should not be reached.");
    } break;
    }
}

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
    std::vector<simgrid::s4u::Host *> & hosts_to_use,
    std::vector<Machine*> & machines_to_use)
{
    hosts_to_use.clear();
    hosts_to_use.reserve(nb_executors);
    machines_to_use.clear();
    machines_to_use.reserve(nb_executors);

    const auto & machines = context->machines.machines();

    using namespace batprotocol::fb;
    switch (predefined_strategy)
    {
    case PredefinedExecutorPlacementStrategy_SpreadOverHostsFirst: {
        auto machine_id_it = allocated_hosts.elements_begin();
        xbt_assert(machine_id_it != allocated_hosts.elements_end(), "cannot create a host list from an empty intervalset");

        for (int i = 0; i < nb_executors; ++i)
        {
            auto * machine = machines.at(*machine_id_it);
            machines_to_use.push_back(machine);
            hosts_to_use.push_back(machine->host);

            ++machine_id_it;
            if (machine_id_it == allocated_hosts.elements_end())
                machine_id_it = allocated_hosts.elements_begin();
        }
    } break;
    case PredefinedExecutorPlacementStrategy_FillOneHostCoresFirst: {

        while (hosts_to_use.size() < static_cast<size_t>(nb_executors))
        {
            for (auto machine_id_it = allocated_hosts.elements_begin(); machine_id_it != allocated_hosts.elements_end(); ++machine_id_it)
            {
                auto machine = machines.at(*machine_id_it);
                const int machine_nb_cores = machine->host->get_core_count();
                for (int core_i = 0; core_i < machine_nb_cores ; ++core_i)
                {
                    hosts_to_use.push_back(machine->host);

                    if (hosts_to_use.size() >= static_cast<size_t>(nb_executors))
                        return;
                }
            }
        }
    } break;
    }
}

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
    std::vector<Machine*> & machines_to_use)
{
    xbt_assert(nb_executors == static_cast<int>(custom_mapping.size()), "inconsistency: nb_executors=%d while custom_mapping has size=%zu", nb_executors, custom_mapping.size());

    hosts_to_use.clear();
    hosts_to_use.reserve(nb_executors);
    machines_to_use.clear();
    machines_to_use.reserve(nb_executors);

    const unsigned int allocated_hosts_size = allocated_hosts.size();
    const int custom_mapping_size = custom_mapping.size();
    for (int i = 0; i < custom_mapping_size; ++i)
    {
        auto & index_in_allocated_hosts = custom_mapping[i];
        xbt_assert(index_in_allocated_hosts < allocated_hosts_size, "bad custom mapping at index=%d: value=%d not in [O,allocated_hosts.size()[ ; allocated_hosts.size()=%u", i, index_in_allocated_hosts, allocated_hosts_size);
        auto machine_id = allocated_hosts[index_in_allocated_hosts];
        auto * machine = context->machines.machines().at(machine_id);
        machines_to_use.push_back(machine);
        hosts_to_use.push_back(machine->host);
    }
}

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
    std::vector<Machine*> & machines_to_use)
{
    if (allocation->use_predefined_strategy)
    {
        hosts_from_predefined_strategy(context, nb_executors, allocation->hosts, allocation->predefined_strategy, hosts_to_use, machines_to_use);
    }
    else
    {
        hosts_from_custom_executor_to_host_mapping(context, nb_executors, allocation->hosts, allocation->custom_mapping, hosts_to_use, machines_to_use);
    }
}

/**
 * @brief Determine how many executors (=SimGrid actors) should execute a given task
 * @param[in] btask The BatTask to execute
 * @param[in] alloc_placement The allocation/placement of the profile
 * @return The number of executors to execute the profile
 */
int determine_task_nb_executors(
    const BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & alloc_placement)
{
    JobPtr job(btask->parent_job);

    if (btask->profile->is_rigid())
    {
        int nb_executors = job->requested_nb_res;
        if (!alloc_placement->use_predefined_strategy)
        {
            xbt_assert(alloc_placement->custom_mapping.size() == static_cast<size_t>(nb_executors),
                "inconsistent placement for job='%s': profile '%s' is rigid (profile_type='%s') but user-given custom mapping has size=%zu, which is not equal to nb_res=%d requested by the job",
                job->id.to_cstring(),
                btask->profile->name.c_str(), profile_type_to_string(btask->profile->type).c_str(),
                alloc_placement->custom_mapping.size(),
                job->requested_nb_res);
        }

        return nb_executors;
    }

    // Profile is NOT rigid, it can adapt to any number of allocated executors.
    if (alloc_placement->use_predefined_strategy)
    {
        return job->requested_nb_res;
    }
    else
    {
        return alloc_placement->custom_mapping.size();
    }
}

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
    const BatsimContext * context)
{
    const Machine * storage_machine = nullptr;
    auto it = storage_mapping.find(storage_label);
    if (it != storage_mapping.end())
    {
        const int storage_machine_id = it->second;
        storage_machine = context->machines[storage_machine_id];
    }
    else
    {
        // The storage label was not overridden by the job execution parameters.
        // This means a storage with the profile default name should exist in the platform.
        storage_machine = context->machines.machine_by_name_or_null(storage_label);
        xbt_assert(storage_machine != nullptr,
            "cannot find a machine with name='%s'. you must either give a storage mapping when executing the job for storage='%s' or have a host with name='%s' in your platform. if you do both, the execution mapping will be tried first.",
            storage_label.c_str(), storage_label.c_str(), storage_label.c_str()
        );
    }

    xbt_assert(storage_machine->permissions == Permissions::STORAGE,
        "trying to use machine (id=%d, name='%s') as storage for storage label='%s', but the machine is not a storage",
        storage_machine->id, storage_machine->name.c_str(), storage_label.c_str()
    );

    return storage_machine;
}

void smpi_trace_replay_actor(JobPtr job, ReplaySmpiProfileData * profile_data, const std::string & termination_mbox_name, int rank)
{
    try
    {
        // Prepare data for smpi_replay_run
        char * str_instance_id = nullptr;
        int ret = asprintf(&str_instance_id, "%s", job->id.to_cstring());
        (void) ret; // Avoids a warning if assertions are ignored
        xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

        XBT_INFO("Replaying rank %d of job %s (SMPI)", rank, job->id.to_cstring());
        smpi_replay_run(str_instance_id, rank, 0, profile_data->trace_filenames[static_cast<size_t>(rank)].c_str());
        XBT_INFO("Replaying rank %d of job %s (SMPI) done", rank, job->id.to_cstring());

        // Tell parent process that replay has finished for this rank.
        auto mbox = simgrid::s4u::Mailbox::by_name(termination_mbox_name);
        auto rank_copy = new unsigned int;
        *rank_copy = static_cast<unsigned int>(rank);
        mbox->put(static_cast<void*>(rank_copy), 4);
    }
    catch (const simgrid::NetworkFailureException & e)
    {
        XBT_INFO("Caught a NetworkFailureException caught: %s", e.what());
    }
}

/**
 * @brief Replays a usage over time trace action (one line)
 * @param[in] action The action to replay
 */
void usage_trace_replayer(simgrid::xbt::ReplayAction & action)
{
    double usage = std::stod(action[2]);
    double flops = std::stod(action[3]);
    xbt_assert(isfinite(usage) && usage >= 0.0 && usage <= 1.0, "invalid usage read: %g not in [0,1]", usage);
    xbt_assert(isfinite(flops) && flops >= 0.0, "invalid flops read: %g not positive and finite", flops);

    // compute how many cores should be used depending on usage and on which host is used
    const double nb_cores = simgrid::s4u::this_actor::get_host()->get_core_count();
    const int nb_cores_to_use = std::max(round(usage * nb_cores), 1.0); // use at least 1 core, otherwise using flops is impossible

    // generate ptask
    std::vector<simgrid::s4u::Host*> hosts_to_use(nb_cores_to_use, simgrid::s4u::this_actor::get_host());
    std::vector<double> computation_vector(nb_cores_to_use, flops);
    std::vector<double> communication_matrix;

    // execute ptask
    simgrid::s4u::ExecPtr ptask = simgrid::s4u::this_actor::exec_init(hosts_to_use, computation_vector, communication_matrix);
    ptask->start();
    ptask->wait();
}

/**
 * @brief The actor that replays a usage trace
 * @param[in] job The job whose trace is from
 * @param[in] data The profile data of the job
 * @param[in] termination_mbox_name The mailbox to use to synchronize the job termination
 * @param[in] rank The rank of the actor of the job
 */
void usage_trace_replay_actor(JobPtr job, ReplayUsageProfileData * data, const std::string & termination_mbox_name, int rank)
{
    try
    {
        // Prepare data for replay_runner
        char * str_rank = nullptr;
        int ret = asprintf(&str_rank, "%d", rank);
        (void) ret; // Avoids a warning if assertions are ignored
        xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

        XBT_INFO("Replaying rank %d of job %s (usage trace)", rank, job->id.to_cstring());
        simgrid::xbt::replay_runner(str_rank, data->trace_filenames[static_cast<size_t>(rank)].c_str());
        XBT_INFO("Replaying rank %d of job %s (usage trace) done", rank, job->id.to_cstring());

        // Tell parent process that replay has finished for this rank.
        auto mbox = simgrid::s4u::Mailbox::by_name(termination_mbox_name);
        auto rank_copy = new unsigned int;
        *rank_copy = static_cast<unsigned int>(rank);
        mbox->put(static_cast<void*>(rank_copy), 4);
    }
    catch (const simgrid::NetworkFailureException & e)
    {
        XBT_INFO("Caught a NetworkFailureException caught: %s", e.what());
    }
}


/**
 * @brief Execute a task that corresponds to a trace replay profile
 * @param[in,out] btask The task to execute. Progress information is stored within it.
 * @param[in] allocation Where the task should be executed
 * @param[in,out] remaining_time The remaining time of the current task. It will be automatically killed if 0 is reached.
 * @param[in] context The BatsimContext
 * @return The profile return code on success, -1 on timeout (remaining time reached 0), -2 on task cancelled (issued by another SimGrid actor)
 */
int execute_trace_replay(
    BatTask * btask,
    const std::shared_ptr<AllocationPlacement> & allocation,
    double * remaining_time,
    BatsimContext * context)
{
    auto * data = static_cast<ReplaySmpiProfileData *>(btask->profile->data);
    auto profile = btask->profile;
    auto job = JobPtr(btask->parent_job);

    unsigned int nb_replay_actors = static_cast<unsigned int>(data->trace_filenames.size());

    // Determine which hosts should be used to execute the trace replay
    std::vector<simgrid::s4u::Host*> hosts_to_use;
    std::vector<Machine *> machines_to_use;
    hosts_from_alloc_placement(context, nb_replay_actors, allocation, hosts_to_use, machines_to_use);

    // Use a mailbox so that child actors tell us when they have finished.
    std::map<unsigned int, simgrid::s4u::ActorPtr> child_actors;
    const std::string termination_mbox_name = simgrid::s4u::this_actor::get_name() + "_smpi_termination";
    auto termination_mbox = simgrid::s4u::Mailbox::by_name(termination_mbox_name);

    xbt_assert(nb_replay_actors == hosts_to_use.size(),
        "Cannot execute job='%s': Trace requires %d replay actors but the given allocation/placement tells to use %zu executors",
        job->id.to_cstring(), nb_replay_actors, hosts_to_use.size()
    );

    if (profile->type == ProfileType::REPLAY_USAGE)
    {
        std::unordered_set<simgrid::s4u::Host*> unique_hosts;
        for (simgrid::s4u::Host * host : hosts_to_use)
            unique_hosts.insert(host);

        xbt_assert(unique_hosts.size() == hosts_to_use.size(),
                   "Cannot execute job='%s': Trace requires %d replay actors that should all be placed on different SimGrid hosts, but the given allocation/placement uses %zu different hosts",
                   job->id.to_cstring(), nb_replay_actors, unique_hosts.size());
    }

    for (unsigned int rank = 0; rank < nb_replay_actors; ++rank)
    {
        std::string actor_name = job->id.to_string() + "_" + std::to_string(rank);
        simgrid::s4u::Host* host_to_use = hosts_to_use[rank];

        simgrid::s4u::ActorPtr actor = nullptr;
        if (profile->type == ProfileType::REPLAY_SMPI)
        {
            auto * data = static_cast<ReplaySmpiProfileData *>(profile->data);
            actor = simgrid::s4u::Actor::create(actor_name, host_to_use, smpi_trace_replay_actor, job, data, termination_mbox_name, rank);
        }
        else if (profile->type == ProfileType::REPLAY_USAGE)
        {
            auto * data = static_cast<ReplayUsageProfileData *>(profile->data);
            actor = simgrid::s4u::Actor::create(actor_name, host_to_use, usage_trace_replay_actor, job, data, termination_mbox_name, rank);
        }

        child_actors[rank] = actor;
        job->execution_actors.insert(actor);
    }

    const bool has_walltime = (*remaining_time >= 0);

    // Wait until all child actors have finished.
    while (!child_actors.empty())
    {
        try
        {
            const double time_before_get = simgrid::s4u::Engine::get_clock();

            unsigned int * finished_rank = nullptr;
            if (has_walltime)
            {
                finished_rank = termination_mbox->get<unsigned int>(*remaining_time);
            }
            else
            {
                finished_rank = termination_mbox->get<unsigned int>();
            }

            xbt_assert(child_actors.count(*finished_rank) == 1, "Internal error: unexpected rank received (%u)", *finished_rank);
            job->execution_actors.erase(child_actors[*finished_rank]);
            child_actors.erase(*finished_rank);
            delete finished_rank;

            if (has_walltime)
            {
                *remaining_time = *remaining_time - (simgrid::s4u::Engine::get_clock() - time_before_get);
            }
        }
        catch (const simgrid::TimeoutException&)
        {
            XBT_DEBUG("Timeout reached while executing SMPI profile '%s' (job's walltime reached).", job->profile->name.c_str());

            // Kill all remaining child actors.
            for (auto mit : child_actors)
            {
                auto child_actor = mit.second;
                job->execution_actors.erase(child_actor);
                child_actor->kill();
            }
            child_actors.clear();

            return -1;
        }
    }

    return profile->return_code;
}

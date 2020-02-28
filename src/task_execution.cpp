/**
 * @file task_execution.cpp
 * @brief Contains functions related to the execution of the parallel profile tasks
 */

#include <simgrid/s4u.hpp>

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
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 */
void generate_parallel_task(std::vector<double>& computation_amount,
                            std::vector<double>& communication_amount,
                            unsigned int nb_res,
                            void * profile_data)
{
    ParallelProfileData* data = (ParallelProfileData*)profile_data;
    xbt_assert(nb_res == data->nb_res,
            "the number of resources given by the allocation (%d) is different "
            "from the number of resouces given by the profile data (%d)",
            nb_res, data->nb_res);

    // Prepare buffers
    computation_amount.resize(nb_res, 0);
    communication_amount.resize(nb_res*nb_res, 0);

    // Retrieve the matrices from the profile
    memcpy(computation_amount.data(), data->cpu, sizeof(double) * nb_res);
    memcpy(communication_amount.data(), data->com, sizeof(double) * nb_res * nb_res);
}

/**
 * @brief Generate the communication and computaion matrix for the
 *        parallel homogeneous task profile. Also set the prefix name of the task.
 * @param[out] computation_amount the computation matrix to be simulated by the parallel task
 * @param[out] communication_amount the communication matrix to be simulated by the parallel task
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 */
void generate_parallel_homogeneous(std::vector<double>& computation_amount,
                                   std::vector<double>& communication_amount,
                                   unsigned int nb_res,
                                   void * profile_data)
{
    ParallelHomogeneousProfileData* data = (ParallelHomogeneousProfileData*)profile_data;

    double cpu = data->cpu;
    double com = data->com;

    // Prepare buffers
    computation_amount.reserve(nb_res);
    if (com > 0)
    {
        communication_amount.reserve(nb_res*nb_res);
    }
    else
    {
        communication_amount.clear();
    }

    // Let us fill the local computation and communication matrices
    for (unsigned int y = 0; y < nb_res; ++y)
    {
        computation_amount.push_back(cpu);
        if (com > 0)
        {
            for (unsigned int x = 0; x < nb_res; ++x)
            {
                if (x == y)
                {
                    communication_amount.push_back(0);
                }
                else
                {
                    communication_amount.push_back(com);
                }
            }
        }
    }
}

/**
 * @brief Generate the communication vector and computation matrix for the
 *        parallel homogeneous total amount task profile.
 *
 * @param[out] computation_amount the computation matrix to be simulated by the parallel task
 * @param[out] communication_amount the communication matrix to be simulated by the parallel task
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 *
 * @details It is like homogeneous profile but instead of giving what has
 *          to be done per host, the user gives the total amounts that should be spread
 *          homogeneously across the hosts.
 */
void generate_parallel_homogeneous_total_amount(std::vector<double>& computation_amount,
                                                std::vector<double>& communication_amount,
                                                unsigned int nb_res,
                                                void * profile_data)
{
    ParallelHomogeneousTotalAmountProfileData* data = (ParallelHomogeneousTotalAmountProfileData*)profile_data;

    const double spread_cpu = data->cpu / nb_res;
    const double spread_com = data->com / nb_res;

    // Prepare buffers
    computation_amount.reserve(nb_res);
    if (spread_com > 0)
    {
        communication_amount.reserve(nb_res*nb_res);
    }
    else
    {
        communication_amount.clear();
    }

    // Fill the local computation and communication matrices
    for (unsigned int y = 0; y < nb_res; ++y)
    {
        computation_amount.push_back(spread_cpu);
        if (spread_com > 0)
        {
            for (unsigned int x = 0; x < nb_res; ++x)
            {
                if (x == y)
                {
                    communication_amount.push_back(0);
                }
                else
                {
                    communication_amount.push_back(spread_com);
                }
            }
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
void generate_parallel_homogeneous_with_pfs(std::vector<double>& computation_amount,
                                            std::vector<double>& communication_amount,
                                            std::vector<simgrid::s4u::Host*> & hosts_to_use,
                                            const std::map<std::string, int> * storage_mapping,
                                            void * profile_data,
                                            BatsimContext * context)
{
    ParallelHomogeneousPFSProfileData* data = (ParallelHomogeneousPFSProfileData*) profile_data;
    const char * error_prefix = "Cannot generate a homogeneous parallel task with pfs: ";

    // The PFS machine will also be used
    unsigned int nb_res = hosts_to_use.size() + 1;
    unsigned int pfs_id = nb_res - 1;

    // Add the pfs_machine
    int pfs_machine_id;
    xbt_assert(storage_mapping != nullptr, "%s: storage mapping is null but the code uses it!", error_prefix);
    if (storage_mapping->empty())
    {
        if (context->machines.storage_machines().size() == 1)
        {
            // No label given: Use the only storage available
            pfs_machine_id = context->machines.storage_machines().at(0)->id;
        }
        else
        {
            xbt_assert(false, "%sNo storage/host mapping given and there is no (or more than one) storage node available", error_prefix);
        }
    }
    else
    {
        xbt_assert(storage_mapping->find(data->storage_label) != storage_mapping->end(),
            "%s: Unknown storage label='%s'", error_prefix, data->storage_label.c_str());
        pfs_machine_id = storage_mapping->at(data->storage_label);

        const Machine * pfs_machine = context->machines[pfs_machine_id];
        xbt_assert(pfs_machine->permissions == Permissions::STORAGE,
            "%sThe host(id=%d, name='%s') pointed to by label='%s' is not a storage host",
            error_prefix, pfs_machine_id, pfs_machine->name.c_str(), data->storage_label.c_str());
    }
    hosts_to_use.push_back(context->machines[pfs_machine_id]->host);

    // Prepare buffers
    computation_amount.reserve(nb_res);
    bool do_comm = data->bytes_to_read > 0 || data->bytes_to_write > 0;
    if (do_comm)
    {
        communication_amount.reserve(nb_res*nb_res);
    }
    else
    {
        communication_amount.clear();
    }

    // Let us fill the local computation and communication matrices
    for (unsigned int row = 0; row < nb_res; ++row)
    {
        computation_amount.push_back(0);
        if (do_comm)
        {
            for (unsigned int col = 0; col < nb_res; ++col)
            {
                // No intra node comm and no inter node comm if it's not the pfs
                if (col == row or (col != pfs_id and row != pfs_id))
                {
                    communication_amount.push_back(0);
                }
                // Writes
                else if (col == pfs_id)
                {
                    communication_amount.push_back(data->bytes_to_write);
                }
                // Reads
                else if (row == pfs_id)
                {
                    communication_amount.push_back(data->bytes_to_read);
                }
            }
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
void generate_data_staging_task(std::vector<double>&  computation_amount,
                                std::vector<double>& communication_amount,
                                std::vector<simgrid::s4u::Host*> & hosts_to_use,
                                const std::map<std::string, int> * storage_mapping,
                                void * profile_data,
                                BatsimContext * context)
{
    DataStagingProfileData * data = (DataStagingProfileData*) profile_data;
    const char * error_prefix = "Cannot generate a data staging task: ";

    double cpu = 0;
    double nb_bytes = data->nb_bytes;

    // The PFS machine will also be used
    unsigned int nb_res = 2;
    unsigned int pfs_id = nb_res - 1;

    // reset the alloc to use only IO nodes
    hosts_to_use.clear();

    // Add the storage machines
    xbt_assert(storage_mapping != nullptr, "%s: storage mapping is null but the code uses it!", error_prefix);
    xbt_assert(storage_mapping->find(data->from_storage_label) != storage_mapping->end(),
        "%s: Unknown storage label='%s'", error_prefix, data->from_storage_label.c_str());
    int from_machine_id = storage_mapping->at(data->from_storage_label);
    const Machine * from_machine = context->machines[from_machine_id];
    xbt_assert(from_machine->permissions == Permissions::STORAGE,
        "%sThe host(id=%d, name='%s') pointed to by label='%s' is not a storage host",
        error_prefix, from_machine_id, from_machine->name.c_str(), data->from_storage_label.c_str());

    xbt_assert(storage_mapping->find(data->to_storage_label) != storage_mapping->end(),
        "%s: Unknown storage label='%s'", error_prefix, data->to_storage_label.c_str());
    int to_machine_id = storage_mapping->at(data->to_storage_label);
    const Machine * to_machine = context->machines[to_machine_id];
    xbt_assert(to_machine->permissions == Permissions::STORAGE,
        "%sThe host(id=%d, name='%s') pointed to by label='%s' is not a storage host",
        error_prefix, to_machine_id, to_machine->name.c_str(), data->to_storage_label.c_str());

    hosts_to_use.push_back(context->machines[from_machine_id]->host);
    hosts_to_use.push_back(context->machines[to_machine_id]->host);

    // Prepare buffers
    computation_amount.reserve(nb_res);
    if (nb_bytes > 0)
    {
        communication_amount.reserve(nb_res*nb_res);
    }
    else
    {
        communication_amount.clear();
    }

    // Let us fill the local computation and communication matrices
    for (unsigned int row = 0; row < nb_res; ++row)
    {
        computation_amount.push_back(cpu);
        if (nb_bytes > 0)
        {
            for (unsigned int col = 0; col < nb_res; ++col)
            {
                // Communications are done towards the last resource
                if (col == row or col != pfs_id)
                {
                    communication_amount.push_back(0);
                }
                else
                {
                    communication_amount.push_back(nb_bytes);
                }
            }
        }
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
 * @brief
 * @param[out] computation_vector The computation vector to be simulated by the parallel task
 * @param[out] communication_matrix The communication matrix to be simulated by the parallel task
 * @param[in,out] hosts_to_use The list of host to be used by the task
 * @param[in] profile The profile to be converted to a compute/comm matrix
 * @param[in] storage_mapping The storage mapping
 * @param[in] context The BatsimContext
 */
void generate_matrices_from_profile(std::vector<double>& computation_vector,
                                    std::vector<double>& communication_matrix,
                                    std::vector<simgrid::s4u::Host*> & hosts_to_use,
                                    ProfilePtr profile,
                                    const std::map<std::string, int> * storage_mapping,
                                    BatsimContext * context)
{

    unsigned int nb_res = hosts_to_use.size();

    XBT_DEBUG("Number of hosts to use: %d", nb_res);

    switch(profile->type)
    {
    case ProfileType::PARALLEL:
        generate_parallel_task(computation_vector,
                                   communication_matrix,
                                   nb_res,
                                   profile->data);
        break;
    case ProfileType::PARALLEL_HOMOGENEOUS:
        generate_parallel_homogeneous(computation_vector,
                                          communication_matrix,
                                          nb_res,
                                          profile->data);
        break;
    case ProfileType::PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT:
        generate_parallel_homogeneous_total_amount(computation_vector,
                                                       communication_matrix,
                                                       nb_res,
                                                       profile->data);
        break;
    case ProfileType::PARALLEL_HOMOGENEOUS_PFS:
        generate_parallel_homogeneous_with_pfs(computation_vector,
                                                   communication_matrix,
                                                   hosts_to_use,
                                                   storage_mapping,
                                                   profile->data,
                                                   context);
        break;
    case ProfileType::DATA_STAGING:
        generate_data_staging_task(computation_vector,
                                        communication_matrix,
                                        hosts_to_use,
                                        storage_mapping,
                                        profile->data,
                                        context);
        break;
    default:
        xbt_die("Should not be reached.");
    }
}

/**
 * @brief Checks if the machines allocated to a ptask can execute it
 * @param[in] alloc The machines on which the ptask should run
 * @param[in] computation_matrix The ptask communication matrix
 * @param[in] context The BatsimContext
 */
void check_ptask_execution_permission(const IntervalSet & alloc,
                                      const std::vector<double>& computation_matrix,
                                      BatsimContext * context)
{
    // TODO: simplify the roles because it is very simple in the end
    // TODO: Enforce role permission

    // TODO: handle mapping (ptasks can be executed with non-unique hosts)
    for (unsigned int i = 0; i < alloc.size(); i++)
    {
        int machine_id = alloc[i];
        XBT_DEBUG("enforcing permission for machine id: %d", machine_id);
        const Machine * machine = context->machines[machine_id];
        // Check if is 0 +- epsilon
        if (std::abs(computation_matrix[i]) > 1e-10)
        {
            XBT_DEBUG("found computation: %.17g", computation_matrix[i]);
            xbt_assert(machine->permissions == Permissions::COMPUTE_NODE,
                "Some computation (%g) is assigned to storage node (id=%d, name='%s')",
                computation_matrix[i], machine_id, machine->name.c_str());
        }
    }
}

int execute_parallel_task(BatTask * btask,
                     const SchedulingAllocation* allocation,
                     double * remaining_time,
                     BatsimContext * context)
{
    auto profile = btask->profile;
    std::vector<simgrid::s4u::Host*> hosts_to_use = allocation->hosts;

    std::vector<double> computation_vector;
    std::vector<double> communication_matrix;

    string task_name = profile_type_to_string(profile->type) + '_' + static_cast<JobPtr>(btask->parent_job)->id.to_string() +
                       "_" + btask->profile->name;
    XBT_DEBUG("Generating comm/compute matrix for task '%s' with allocation %s",
            task_name.c_str(), allocation->machine_ids.to_string_hyphen().c_str());

    generate_matrices_from_profile(computation_vector,
                                  communication_matrix,
                                  hosts_to_use,
                                  profile,
                                  & allocation->storage_mapping,
                                  context);

    debug_print_ptask(computation_vector, communication_matrix,
            hosts_to_use.size(), allocation->machine_ids, allocation->mapping);

    check_ptask_execution_permission(allocation->machine_ids, computation_vector, context);

    //FIXME: This will not work for the PFS profiles
    // Manage additional io job
    if (btask->io_profile != nullptr)
    {
        auto io_profile = btask->io_profile;
        std::vector<double> io_computation_vector;
        std::vector<double> io_communication_matrix;

        XBT_DEBUG("Generating comm/compute matrix for IO with allocation: %s",
                allocation->io_allocation.to_string_hyphen().c_str());
        std::vector<simgrid::s4u::Host*> io_hosts = allocation->io_hosts;
        generate_matrices_from_profile(io_computation_vector,
                                      io_communication_matrix,
                                      io_hosts,
                                      io_profile,
                                      nullptr,
                                      context);
        debug_print_ptask(io_computation_vector, io_communication_matrix,
                io_hosts.size(), allocation->io_allocation);

        // merge the two profiles
        // First get part of the allocation that do change or not in the job
        IntervalSet immut_job_alloc = allocation->machine_ids - allocation->io_allocation;
        IntervalSet immut_io_alloc = allocation->io_allocation - allocation->machine_ids;
        IntervalSet to_merge_alloc = allocation->machine_ids & allocation->io_allocation;
        IntervalSet new_alloc = allocation->machine_ids + allocation->io_allocation;

        // FIXME this does not work for profiles that changes the number of hosts: where the allocation and the host to use
        // are different
        // Maybe this and the IO profiles should be merged to simplify implementation
        XBT_DEBUG("Job+IO allocation: %s", new_alloc.to_string_hyphen().c_str());

        //Generate the new list of hosts
        vector<simgrid::s4u::Host*> new_hosts_to_use;
        for (unsigned int i = 0; i < new_alloc.size(); i++)
        {
            int machine_id = new_alloc[i];
            new_hosts_to_use.push_back(context->machines[machine_id]->host);
        }

        // Generate the new matrices
        unsigned int nb_res = new_hosts_to_use.size();

        // Prepare buffers
        std::vector<double> new_computation_vector(nb_res, 0);
        std::vector<double> new_communication_matrix(nb_res*nb_res, 0);

        // Fill the computation and communication matrices
        int k = 0;
        int col_job_host_index = 0;
        int row_job_host_index = 0;
        int col_io_host_index = 0;
        int row_io_host_index = 0;
        bool col_only_in_job;
        bool col_only_in_io;
        for (unsigned int col = 0; col < nb_res; ++col)
        {
            col_only_in_job = false;
            col_only_in_io = false;
            int curr_machine = new_alloc[col];
            XBT_DEBUG("Current machine in generation: %d", col);
            // Fill computation vector
            if (to_merge_alloc.contains(curr_machine))
            {
                new_computation_vector[col] = computation_vector[col_job_host_index++] + io_computation_vector[col_io_host_index++];
            }
            else if (immut_job_alloc.contains(curr_machine))
            {
                new_computation_vector[col] = computation_vector[col_job_host_index++];
                col_only_in_job = true;
            }
            else if (immut_io_alloc.contains(curr_machine))
            {
                new_computation_vector[col] = io_computation_vector[col_io_host_index++];
                col_only_in_io = true;
            }
            else
            {
                xbt_assert(false, "This should not happen");
            }

            // Fill communication matrix with merged values
            for (unsigned int row = 0; row < nb_res; ++row)
            {
                if (to_merge_alloc.contains(new_alloc[row]))
                {
                    if (col_only_in_job){
                        if (!communication_matrix.empty())
                        {
                            new_communication_matrix[k] = communication_matrix[row_job_host_index++];
                        }
                        else
                        {
                            new_communication_matrix[k] = 0;
                        }
                    }
                    else if (col_only_in_io){
                        new_communication_matrix[k] = io_communication_matrix[row_io_host_index++];
                    }
                    else {
                        if (!communication_matrix.empty())
                        {
                            new_communication_matrix[k] = communication_matrix[row_job_host_index++] + io_communication_matrix[row_io_host_index++];
                        }
                        else
                        {
                            new_communication_matrix[k] = io_communication_matrix[row_io_host_index++];
                        }
                    }
                }
                else if (immut_job_alloc.contains(new_alloc[row]))
                {
                    if (col_only_in_io or communication_matrix.empty()){
                        new_communication_matrix[k] = 0;
                    }
                    else
                    {
                        new_communication_matrix[k] = communication_matrix[row_job_host_index++];
                    }
                }
                else if (immut_io_alloc.contains(new_alloc[row]))
                {
                    if (col_only_in_job){
                        new_communication_matrix[k] = 0;
                    }
                    else
                    {
                        new_communication_matrix[k] = io_communication_matrix[row_io_host_index++];
                    }
                }
                else
                {
                    xbt_assert(false, "This should not happen");
                }
                k++;
            }
        }

        // update variables with merged matrix
        communication_matrix = new_communication_matrix;
        computation_vector = new_computation_vector;
        hosts_to_use = new_hosts_to_use;
        // TODO Free old job and io structures
        XBT_DEBUG("Merged Job+IO matrices");
        debug_print_ptask(computation_vector, communication_matrix, hosts_to_use.size(), new_alloc);

        check_ptask_execution_permission(new_alloc, computation_vector, context);
    }


    // Create the parallel task
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
        ptask->start();
        ptask->wait();
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
        *remaining_time = *remaining_time - (simgrid::s4u::Engine::get_clock() - time_before_execute);
    }

    XBT_DEBUG("Task '%s' finished in %f", task_name.c_str(),
        simgrid::s4u::Engine::get_clock() - time_start);

    return ret;
}

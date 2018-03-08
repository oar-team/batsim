/**
 * @file task_execution.cpp
 * @brief Contains functions related to the execution of the MSG profile tasks
 */

#include <simgrid/msg.h>
#include "jobs.hpp"
#include "profiles.hpp"
#include "ipp.hpp"
#include "context.hpp"
#include "jobs_execution.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(task_execution, "task_execution"); //!< Logging

using namespace std;

/**
 * @brief Generate the communication and computaion matrix for the msg
 * parallel task profile. It also set the prefix name of the task.
 * @param[out] task_name_prefix the prefix to add to the task name
 * @param[out] computation_amount the computation matrix to be simulated by the msg task
 * @param[out] communication_amount the communication matrix to be simulated by the msg task
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 */
void generate_msg_parallel_task(std::string & task_name_prefix,
                                double *& computation_amount,
                                double *& communication_amount,
                                unsigned int nb_res,
                                void * profile_data)
{
    task_name_prefix = "p ";
    MsgParallelProfileData* data = (MsgParallelProfileData*)profile_data;
    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = xbt_new(double, nb_res* nb_res);

    // Retrieve the matrices from the profile
    memcpy(computation_amount, data->cpu, sizeof(double) * nb_res);
    memcpy(communication_amount, data->com, sizeof(double) * nb_res * nb_res);
}

/**
 * @brief Generates the communication and computaion matrix for the msg
 *        parallel homogeneous task profile. It also set the prefix name of the task.
 * @param[out] task_name_prefix the prefix to add to the task name
 * @param[out] computation_amount the computation matrix to be simulated by the msg task
 * @param[out] communication_amount the communication matrix to be simulated by the msg task
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 */
void generate_msg_parallel_homogeneous(std::string & task_name_prefix,
                                       double *& computation_amount,
                                       double *& communication_amount,
                                       unsigned int nb_res,
                                       void * profile_data)
{
    task_name_prefix = "phg ";
    MsgParallelHomogeneousProfileData* data = (MsgParallelHomogeneousProfileData*)profile_data;

    double cpu = data->cpu;
    double com = data->com;

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (com > 0)
    {
        communication_amount = xbt_new(double, nb_res* nb_res);
    }

    // Let us fill the local computation and communication matrices
    int k = 0;
    for (unsigned int y = 0; y < nb_res; ++y)
    {
        computation_amount[y] = cpu;
        if (communication_amount != nullptr)
        {
            for (unsigned int x = 0; x < nb_res; ++x)
            {
                if (x == y)
                {
                    communication_amount[k] = 0;
                }
                else
                {
                    communication_amount[k] = com;
                }
                k++;
            }
        }
    }
}

/**
 * @brief Generates the communication vector and computation matrix for the msg
 *        parallel homogeneous total amount task profile.
 *
 * @param[out] task_name_prefix the prefix to add to the task name
 * @param[out] computation_amount the computation matrix to be simulated by the msg task
 * @param[out] communication_amount the communication matrix to be simulated by the msg task
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 *
 * @details It is like homogeneous profile but instead of giving what has
 *          to be done per host, the user gives the total amounts that should be spread
 *          homogeneously across the hosts.
 */
void generate_msg_parallel_homogeneous_total_amount(std::string & task_name_prefix,
                                                    double *& computation_amount,
                                                    double *& communication_amount,
                                                    unsigned int nb_res,
                                                    void * profile_data)
{
    task_name_prefix = "phgt ";
    MsgParallelHomogeneousTotalAmountProfileData* data = (MsgParallelHomogeneousTotalAmountProfileData*)profile_data;

    const double spread_cpu = data->cpu / nb_res;
    const double spread_com = data->com / nb_res;

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (spread_com > 0)
    {
        communication_amount = xbt_new(double, nb_res * nb_res);
    }

    // Fill the local computation and communication matrices
    int k = 0;
    for (unsigned int y = 0; y < nb_res; ++y)
    {
        computation_amount[y] = spread_cpu;
        if (communication_amount != nullptr)
        {
            for (unsigned int x = 0; x < nb_res; ++x)
            {
                if (x == y)
                {
                    communication_amount[k] = 0;
                }
                else
                {
                    communication_amount[k] = spread_com;
                }
                k++;
            }
        }
    }
}

/**
 * @brief Generate the communication and computaion matrix for the msg
 *        parallel homogeneous task profile with pfs.
 *
 * @param[out] task_name_prefix the prefix to add to the task name
 * @param[out] computation_amount the computation matrix to be simulated by the msg task
 * @param[out] communication_amount the communication matrix to be simulated by the msg task
 * @param[in,out] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 * @param[in,out] hosts_to_use the list of host to be used by the task
 * @param[in] context the batsim context
 *
 * @details Note that the number of resource is also altered because of the
 *          pfs node that is addded. It also set the prefix name of the
 *          task.
 */
void generate_msg_parallel_homogeneous_with_pfs(std::string & task_name_prefix,
                                                double *& computation_amount,
                                                double *& communication_amount,
                                                unsigned int & nb_res,
                                                void * profile_data,
                                                BatsimContext * context,
                                                std::vector<msg_host_t> & hosts_to_use)
{
    task_name_prefix = "pfs_tiers ";
    MsgParallelHomogeneousPFSMultipleTiersProfileData* data =
            (MsgParallelHomogeneousPFSMultipleTiersProfileData*)profile_data;

    double cpu = 0;
    double size = data->size;

    // The PFS machine will also be used
    nb_res = nb_res + 1;
    unsigned int pfs_id = nb_res - 1;

    // Add the pfs_machine
    hosts_to_use.push_back(context->machines[data->storage_id]->host);

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (size > 0)
    {
        communication_amount = xbt_new(double, nb_res* nb_res);
    }

    // Let us fill the local computation and communication matrices
    int k = 0;
    for (unsigned int y = 0; y < nb_res; ++y)
    {
        computation_amount[y] = cpu;
        if (communication_amount != nullptr)
        {
            for (unsigned int x = 0; x < nb_res; ++x)
            {
                switch (data->direction)
                {
                case MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::FROM_NODES_TO_STORAGE:
                {
                    // Communications are done towards the PFS host,
                    // which is the last resource (to the storage)
                    if (x != pfs_id)
                    {
                        communication_amount[k] = 0;
                    }
                    else
                    {
                        communication_amount[k] = size;
                    }
                    k++;
                } break;
                case MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::FROM_STORAGE_TO_NODES:
                {
                    // Communications are done towards the job
                    // allocation (from the storage)
                    if (x != pfs_id)
                    {
                        communication_amount[k] = size;
                    }
                    else
                    {
                        communication_amount[k] = 0;
                    }
                    k++;
                } break;
                default:
                    xbt_die("Should not be reached");
                }
            }
        }
    }
}

/**
 * @brief Generate the communication and computaion matrix for the msg
 * data staging task profile.
 * @details Note that the number of resource is also altered because only
 * the pfs and the hpst are involved in the transfer. It also set the prefix
 * name of the task.
 * @param[out] task_name_prefix the prefix to add to the task name
 * @param[out] computation_amount the computation matrix to be simulated by the msg task
 * @param[out] communication_amount the communication matrix to be simulated by the msg task
 * @param[in,out] nb_res the number of resources the task have to run on
 * @param[in] profile_data the profile data
 * @param[in,out] hosts_to_use the list of host to be used by the task
 * @param[in] context the batsim context
 */
void generate_msg_data_staginig_task(string task_name_prefix,
                                     double *&  computation_amount,
                                     double *& communication_amount,
                                     unsigned int & nb_res,
                                     void * profile_data,
                                     BatsimContext * context,
                                     std::vector<msg_host_t> & hosts_to_use)
{
    task_name_prefix = "data_staging ";
    MsgDataStagingProfileData* data = (MsgDataStagingProfileData*)profile_data;

    double cpu = 0;
    double size = data->size;

    // The PFS machine will also be used
    nb_res = 2;
    unsigned int pfs_id = nb_res - 1;

    // reset the alloc to use only IO nodes
    hosts_to_use = std::vector<msg_host_t>();

    // Add the pfs_machine
    hosts_to_use.push_back(context->machines[data->from_storage_id]->host);
    hosts_to_use.push_back(context->machines[data->to_storage_id]->host);

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (size > 0)
    {
        communication_amount = xbt_new(double, nb_res* nb_res);
    }

    // Let us fill the local computation and communication matrices
    int k = 0;
    for (unsigned int y = 0; y < nb_res; ++y)
    {
        computation_amount[y] = cpu;
        if (communication_amount != nullptr)
        {
            for (unsigned int x = 0; x < nb_res; ++x)
            {
                // Communications are done towards the last resource
                if (x != pfs_id)
                {
                    communication_amount[k] = 0;
                }
                else
                {
                    communication_amount[k] = size;
                }
                k++;
            }
        }
    }
}

int execute_msg_task(BatTask * btask,
                     const SchedulingAllocation* allocation,
                     unsigned int nb_res,
                     double * remaining_time,
                     BatsimContext * context,
                     CleanExecuteTaskData * cleanup_data)
{
    std::vector<msg_host_t> hosts_to_use = allocation->hosts;
    Profile * profile = btask->profile;

    double* computation_amount = nullptr;
    double* communication_amount = nullptr;
    string task_name_prefix;
    int ret;

    // Generate communication and computation matrix depending on the profile
    switch(profile->type)
    {
    case ProfileType::MSG_PARALLEL:
        generate_msg_parallel_task(task_name_prefix, computation_amount, communication_amount,
                                   nb_res, profile->data);
        break;
    case ProfileType::MSG_PARALLEL_HOMOGENEOUS:
        generate_msg_parallel_homogeneous(task_name_prefix, computation_amount,
                                          communication_amount, nb_res, profile->data);
        break;
    case ProfileType::MSG_PARALLEL_HOMOGENEOUS_TOTAL_AMOUNT:
        generate_msg_parallel_homogeneous_total_amount(task_name_prefix, computation_amount,
                                                       communication_amount, nb_res, profile->data);
        break;
    case ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS:
        generate_msg_parallel_homogeneous_with_pfs(task_name_prefix, computation_amount,
                                                   communication_amount, nb_res, profile->data,
                                                   context, hosts_to_use);
        break;
    case ProfileType::MSG_DATA_STAGING:
        generate_msg_data_staginig_task(task_name_prefix, computation_amount, communication_amount,
                                        nb_res, profile->data, context, hosts_to_use);
        break;
    default:
        xbt_die("Should not be reached.");
    }

    // Create the MSG task
    string task_name = task_name_prefix + btask->parent_job->id.to_string() +
                       "'" + btask->profile->name + "'";
    XBT_DEBUG("Creating MSG task '%s' on %d resources", task_name.c_str(), nb_res);
    msg_task_t ptask = MSG_parallel_task_create(task_name.c_str(), nb_res,
                                                hosts_to_use.data(), computation_amount,
                                                communication_amount, NULL);

    // If the process gets killed, the following data may need to be freed
    cleanup_data->task = ptask;

    // Keep track of the task to get information on kill
    btask->ptask = ptask;

    // Execute the MSG task (blocking)
    msg_error_t err;
    if (*remaining_time < 0)
    {
        XBT_INFO("Executing task '%s' without walltime", MSG_task_get_name(ptask));
        err = MSG_parallel_task_execute(ptask);
    }
    else
    {
        double time_before_execute = MSG_get_clock();
        XBT_INFO("Executing task '%s' with walltime of %g", MSG_task_get_name(ptask), *remaining_time);
        err = MSG_parallel_task_execute_with_timeout(ptask, *remaining_time);
        *remaining_time = *remaining_time - (MSG_get_clock() - time_before_execute);
    }

    if (err == MSG_OK)
    {
        ret = profile->return_code;
    }
    else if (err == MSG_TIMEOUT)
    {
        ret = -1;
    }
    else
    {
        xbt_die("A task execution had been stopped by an unhandled way (err = %d)", err);
    }

    XBT_INFO("Task '%s' finished", MSG_task_get_name(ptask));
    MSG_task_destroy(ptask);

    // The task has been executed, the data does need to be freed in the cleanup function anymore
    cleanup_data->task = nullptr;

    return ret;
}



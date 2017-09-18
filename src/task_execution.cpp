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


void generate_msg_parallel_task(
    string task_name_prefix,
    double* computation_amount,
    double* communication_amount,
    int nb_res,
    void* profile_data)
{
    task_name_prefix = "p ";
    MsgParallelProfileData* data = (MsgParallelProfileData*)profile_data;
    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = xbt_new(double, nb_res* nb_res);

    // Let us retrieve the matrices from the profile
    memcpy(computation_amount, data->cpu, sizeof(double) * nb_res);
    memcpy(communication_amount, data->com, sizeof(double) * nb_res * nb_res);
}

void generate_msg_parallel_homogeneous(
    string task_name_prefix,
    double* computation_amount,
    double* communication_amount,
    int nb_res,
    void* profile_data)
{
    task_name_prefix = "phg ";
    MsgParallelHomogeneousProfileData* data
        = (MsgParallelHomogeneousProfileData*)profile_data;

    double cpu = data->cpu;
    double com = data->com;

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (com > 0) {
        communication_amount = xbt_new(double, nb_res* nb_res);
    }

    // Let us fill the local computation and communication matrices
    int k = 0;
    for (int y = 0; y < nb_res; ++y) {
        computation_amount[y] = cpu;
        if (communication_amount != nullptr) {
            for (int x = 0; x < nb_res; ++x) {
                if (x == y) {
                    communication_amount[k++] = 0;
                } else {
                    communication_amount[k++] = com;
                }
            }
        }
    }
}

void generate_msg_parallel_homogeneous_with_pfs(
    string task_name_prefix,
    double* computation_amount,
    double* communication_amount,
    int nb_res,
    void* profile_data,
    BatsimContext* context,
    std::vector<msg_host_t> hosts_to_use)
{
    task_name_prefix = "pfs_tiers ";
    MsgParallelHomogeneousPFSMultipleTiersProfileData* data
        = (MsgParallelHomogeneousPFSMultipleTiersProfileData*)profile_data;

    double cpu = 0;
    double size = data->size;

    // The PFS machine will also be used
    nb_res = nb_res + 1;
    int pfs_id = nb_res - 1;

    // Add the pfs_machine
    switch (data->host) {
    case MsgParallelHomogeneousPFSMultipleTiersProfileData::Host::HPST:
        hosts_to_use.push_back(context->machines.hpst_machine()->host);
        break;
    case MsgParallelHomogeneousPFSMultipleTiersProfileData::Host::LCST:
        hosts_to_use.push_back(context->machines.pfs_machine()->host);
        break;
    default:
        xbt_die("Should not be reached");
    }

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (size > 0) {
        communication_amount = xbt_new(double, nb_res* nb_res);
    }

    // Let us fill the local computation and communication matrices
    int k = 0;
    for (int y = 0; y < nb_res; ++y) {
        computation_amount[y] = cpu;
        if (communication_amount != nullptr) {
            for (int x = 0; x < nb_res; ++x) {
                switch (data->direction) {
                case MsgParallelHomogeneousPFSMultipleTiersProfileData::
                    Direction::TO_STORAGE:
                    // Communications are done towards the PFS host,
                    // which is the last resource (to the storage)
                    if (x != pfs_id) {
                        communication_amount[k++] = 0;
                    } else {
                        communication_amount[k++] = size;
                    }
                    break;
                case MsgParallelHomogeneousPFSMultipleTiersProfileData::
                    Direction::FROM_STORAGE:
                    // Communications are done towards the job
                    // allocation (from the storage)
                    if (x != pfs_id) {
                        communication_amount[k++] = size;
                    } else {
                        communication_amount[k++] = 0;
                    }
                    break;
                default:
                    xbt_die("Should not be reached");
                }
            }
        }
    }
}

void generate_msg_data_staginig_task(string task_name_prefix,
    double* computation_amount,
    double* communication_amount,
    int nb_res,
    void* profile_data,
    BatsimContext* context,
    std::vector<msg_host_t> hosts_to_use)
{
    task_name_prefix = "data_staging ";
    MsgDataStagingProfileData* data = (MsgDataStagingProfileData*)profile_data;

    double cpu = 0;
    double size = data->size;

    // The PFS machine will also be used
    nb_res = 2;
    int pfs_id = nb_res - 1;

    // reset the alloc to use only IO nodes
    hosts_to_use = std::vector<msg_host_t>();

    // Add the pfs_machine
    switch (data->direction) {
    case MsgDataStagingProfileData::Direction::LCST_TO_HPST:
        hosts_to_use.push_back(context->machines.pfs_machine()->host);
        hosts_to_use.push_back(context->machines.hpst_machine()->host);
        break;
    case MsgDataStagingProfileData::Direction::HPST_TO_LCST:
        hosts_to_use.push_back(context->machines.hpst_machine()->host);
        hosts_to_use.push_back(context->machines.pfs_machine()->host);
        break;
    default:
        xbt_die("Should not be reached");
    }

    // These amounts are deallocated by SG
    computation_amount = xbt_new(double, nb_res);
    communication_amount = nullptr;
    if (size > 0) {
        communication_amount = xbt_new(double, nb_res* nb_res);
    }

    // Let us fill the local computation and communication matrices
    int k = 0;
    for (int y = 0; y < nb_res; ++y) {
        computation_amount[y] = cpu;
        if (communication_amount != nullptr) {
            for (int x = 0; x < nb_res; ++x) {
                // Communications are done towards the last resource
                if (x != pfs_id) {
                    communication_amount[k++] = 0;
                } else {
                    communication_amount[k++] = size;
                }
            }
        }
    }
}

int execute_msg_task(
        const SchedulingAllocation* allocation,
        int nb_res,
        Job * job,
        double * remaining_time,
        Profile * profile,
        BatsimContext * context,
        CleanExecuteProfileData * cleanup_data)
{
    double* computation_amount = nullptr;
    double* communication_amount = nullptr;
    string task_name_prefix;
    std::vector<msg_host_t> hosts_to_use = allocation->hosts;
    int ret;

    // Generate communication and computation matrix depending on the profile
    if (profile->type == ProfileType::MSG_PARALLEL) {
        generate_msg_parallel_task(
                task_name_prefix,
                computation_amount,
                communication_amount,
                nb_res,
                profile->data);

    } else if (profile->type == ProfileType::MSG_PARALLEL_HOMOGENEOUS) {
        generate_msg_parallel_homogeneous(
                task_name_prefix,
                computation_amount,
                communication_amount,
                nb_res,
                profile->data);
    } else if (profile->type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS) {
        generate_msg_parallel_homogeneous_with_pfs(
                task_name_prefix,
                computation_amount,
                communication_amount,
                nb_res,
                profile->data,
                context,
                hosts_to_use);
    } else if (profile->type == ProfileType::MSG_DATA_STAGING) {
        generate_msg_data_staginig_task(
                task_name_prefix,
                computation_amount,
                communication_amount,
                nb_res,
                profile->data,
                context,
                hosts_to_use);
    }

    // Create the MSG task
    string task_name
        = task_name_prefix + to_string(job->number) + "'" + job->profile + "'";
    XBT_INFO("Creating task '%s'", task_name.c_str());

    msg_task_t ptask = MSG_parallel_task_create(task_name.c_str(), nb_res,
        hosts_to_use.data(), computation_amount, communication_amount, NULL);

    // If the process gets killed, the following data may need to be freed
    cleanup_data->task = ptask;
    // Keep track of the task associated to the job to get information
    job->task = new BatTask(task_name, ptask, *computation_amount, *communication_amount);

    // Execute the MSG task
    double time_before_execute = MSG_get_clock();
    XBT_INFO("Executing task '%s'", MSG_task_get_name(ptask));
    msg_error_t err
        = MSG_parallel_task_execute_with_timeout(ptask, *remaining_time);
    *remaining_time = *remaining_time - (MSG_get_clock() - time_before_execute);

    ret = profile->return_code;
    if (err == MSG_OK) {
    } else if (err == MSG_TIMEOUT) {
        ret = -1;
    } else {
        xbt_die("A task execution had been stopped by an unhandled way "
                "(err = %d)",
            err);
    }

    XBT_INFO("Task '%s' finished", MSG_task_get_name(ptask));
    MSG_task_destroy(ptask);

    // The task has been executed, the data does need to be freed in the
    // cleanup function anymore
    cleanup_data->task = nullptr;

    return ret;
}



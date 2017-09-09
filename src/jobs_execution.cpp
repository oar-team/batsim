/**
 * @file jobs_execution.cpp
 * @brief Contains functions related to the execution of the jobs
 */
#include <regex>

#include "jobs_execution.hpp"
#include "jobs.hpp"

#include <simgrid/plugins/energy.h>

#include <simgrid/msg.h>
#include <smpi/smpi.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(jobs_execution, "jobs_execution"); //!< Logging

using namespace std;

int smpi_replay_process(int argc, char *argv[])
{
    SMPIReplayProcessArguments * args = (SMPIReplayProcessArguments *) MSG_process_get_data(MSG_process_self());

    if (args->semaphore != NULL)
    {
        MSG_sem_acquire(args->semaphore);
    }

    XBT_INFO("Launching smpi_replay_run");
    smpi_replay_run(&argc, &argv);
    XBT_INFO("smpi_replay_run finished");

    if (args->semaphore != NULL)
    {
        MSG_sem_release(args->semaphore);
    }

    args->job->execution_processes.erase(MSG_process_self());
    delete args;
    return 0;
}

int execute_profile(BatsimContext *context,
                    const std::string & profile_name,
                    const SchedulingAllocation * allocation,
                    CleanExecuteProfileData * cleanup_data,
                    double * remaining_time)
{
    Workload * workload = context->workloads.at(allocation->job_id.workload_name);
    Job * job = workload->jobs->at(allocation->job_id.job_number);
    JobIdentifier job_id(workload->name, job->number);
    Profile * profile = workload->profiles->at(profile_name);
    int nb_res = job->required_nb_res;

    if (profile->type == ProfileType::MSG_PARALLEL ||
        profile->type == ProfileType::MSG_PARALLEL_HOMOGENEOUS ||
        profile->type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS ||
        profile->type == ProfileType::MSG_DATA_STAGING)
    {
        double * computation_amount = nullptr;
        double * communication_amount = nullptr;
        string task_name_prefix;
        std::vector<msg_host_t> hosts_to_use = allocation->hosts;

        if (profile->type == ProfileType::MSG_PARALLEL)
        {
            task_name_prefix = "p ";
            MsgParallelProfileData * data = (MsgParallelProfileData *)profile->data;

            // These amounts are deallocated by SG
            computation_amount = xbt_new(double, nb_res);
            communication_amount = xbt_new(double, nb_res*nb_res);

            // Let us retrieve the matrices from the profile
            memcpy(computation_amount, data->cpu, sizeof(double) * nb_res);
            memcpy(communication_amount, data->com, sizeof(double) * nb_res * nb_res);
        }
        else if (profile->type == ProfileType::MSG_PARALLEL_HOMOGENEOUS)
        {
            task_name_prefix = "phg ";
            MsgParallelHomogeneousProfileData * data = (MsgParallelHomogeneousProfileData *)profile->data;

            double cpu = data->cpu;
            double com = data->com;

            // These amounts are deallocated by SG
            computation_amount = xbt_new(double, nb_res);
            communication_amount = nullptr;
            if(com > 0)
            {
                communication_amount = xbt_new(double, nb_res * nb_res);
            }

            // Let us fill the local computation and communication matrices
            int k = 0;
            for (int y = 0; y < nb_res; ++y)
            {
                computation_amount[y] = cpu;
                if(communication_amount != nullptr)
                {
                    for (int x = 0; x < nb_res; ++x)
                    {
                        if (x == y)
                        {
                            communication_amount[k++] = 0;
                        }
                        else
                        {
                            communication_amount[k++] = com;
                        }
                    }
                }
            }
        }
        else if (profile->type == ProfileType::MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS)
        {
            task_name_prefix = "pfs_tiers ";
            MsgParallelHomogeneousPFSMultipleTiersProfileData * data = (MsgParallelHomogeneousPFSMultipleTiersProfileData *)profile->data;

            double cpu = 0;
            double size = data->size;

            // The PFS machine will also be used
            nb_res = nb_res + 1;
            int pfs_id = nb_res - 1;

            // Add the pfs_machine
            switch(data->host)
            {
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
            if(size > 0)
            {
                communication_amount = xbt_new(double, nb_res*nb_res);
            }

            // Let us fill the local computation and communication matrices
            int k = 0;
            for (int y = 0; y < nb_res; ++y)
            {
                computation_amount[y] = cpu;
                if(communication_amount != nullptr)
                {
                    for (int x = 0; x < nb_res; ++x)
                    {
                        switch(data->direction)
                        {
                        case MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::TO_STORAGE:
                            // Communications are done towards the PFS host, which is the last resource (to the storage)
                            if (x != pfs_id)
                            {
                                communication_amount[k++] = 0;
                            }
                            else
                            {
                                communication_amount[k++] = size;
                            }
                            break;
                        case MsgParallelHomogeneousPFSMultipleTiersProfileData::Direction::FROM_STORAGE:
                            // Communications are done towards the job allocation (from the storage)
                            if (x != pfs_id)
                            {
                                communication_amount[k++] = size;
                            }
                            else
                            {
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
        else if (profile->type == ProfileType::MSG_DATA_STAGING)
        {
            task_name_prefix = "data_staging ";
            MsgDataStagingProfileData * data = (MsgDataStagingProfileData *)profile->data;

            double cpu = 0;
            double size = data->size;

            // The PFS machine will also be used
            nb_res = 2;
            int pfs_id = nb_res - 1;

            hosts_to_use = std::vector<msg_host_t>();

            // Add the pfs_machine
            switch(data->direction)
            {
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
            if(size > 0)
            {
                communication_amount = xbt_new(double, nb_res*nb_res);
            }

            // Let us fill the local computation and communication matrices
            int k = 0;
            for (int y = 0; y < nb_res; ++y)
            {
                computation_amount[y] = cpu;
                if(communication_amount != nullptr)
                {
                    for (int x = 0; x < nb_res; ++x)
                    {
                        // Communications are done towards the last resource
                        if (x != pfs_id)
                        {
                            communication_amount[k++] = 0;
                        }
                        else
                        {
                            communication_amount[k++] = size;
                        }
                    }
                }
            }
        }

        string task_name = task_name_prefix + to_string(job->number) + "'" + job->profile + "'";
        XBT_INFO("Creating task '%s'", task_name.c_str());

        msg_task_t ptask = MSG_parallel_task_create(task_name.c_str(),
                                                    nb_res,
                                                    hosts_to_use.data(),
                                                    computation_amount,
                                                    communication_amount, NULL);

        // If the process gets killed, the following data may need to be freed
        cleanup_data->task = ptask;

        double time_before_execute = MSG_get_clock();
        XBT_INFO("Executing task '%s'", MSG_task_get_name(ptask));
        msg_error_t err = MSG_parallel_task_execute_with_timeout(ptask, *remaining_time);
        *remaining_time = *remaining_time - (MSG_get_clock() - time_before_execute);

        int ret = profile->return_code;
        if (err == MSG_OK) {}
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
    else if (profile->type == ProfileType::SEQUENCE)
    {
        SequenceProfileData * data = (SequenceProfileData *) profile->data;

        for (int i = 0; i < data->repeat; i++)
        {
            for (unsigned int j = 0; j < data->sequence.size(); j++)
            {
                int ret_last_profile = execute_profile(context, data->sequence[j], allocation,
                                    cleanup_data, remaining_time);
                if (ret_last_profile != 0)
                {
                    return ret_last_profile;
                }
            }
        }
        return profile->return_code;
    }
    else if (profile->type == ProfileType::SCHEDULER_SEND)
    {
        SchedulerSendProfileData * data = (SchedulerSendProfileData *) profile->data;

        XBT_INFO("Sending message to the scheduler");

        FromJobMessage * message = new FromJobMessage;
        message->job_id = job_id;
        message->message.CopyFrom(data->message, message->message.GetAllocator());

        send_message("server", IPMessageType::FROM_JOB_MSG, (void*)message);

        if (delay_job(data->sleeptime, remaining_time) == -1)
            return -1;

        return profile->return_code;
    }
    else if (profile->type == ProfileType::SCHEDULER_RECV)
    {
        SchedulerRecvProfileData * data = (SchedulerRecvProfileData *) profile->data;

        string profile_to_execute = "";
        bool has_messages = false;

        XBT_INFO("Trying to receive message from scheduler");
        if (job->incoming_message_buffer.empty()) {
            if (data->on_timeout == "") {
                XBT_INFO("Waiting for message from scheduler");
                while (true) {
                    if (delay_job(data->polltime, remaining_time) == -1)
                        return -1;

                    if (!job->incoming_message_buffer.empty()) {
                        XBT_INFO("Finally got message from scheduler");
                        has_messages = true;
                        break;
                    }
                }
            } else {
                XBT_INFO("Timeout on waiting for message from scheduler");
                profile_to_execute = data->on_timeout;
            }
        } else {
            has_messages = true;
        }
        
        if (has_messages) {
            string first_message = job->incoming_message_buffer.front();
            job->incoming_message_buffer.pop_front();

            regex msg_regex(data->regex);
            if (regex_match(first_message, msg_regex)) {
                XBT_INFO("Message from scheduler matches");
                profile_to_execute = data->on_success;
            } else {
                XBT_INFO("Message from scheduler does not match");
                profile_to_execute = data->on_failure;
            }
        }

        if (profile_to_execute != "") {
            XBT_INFO("Execute profile: %s", profile_to_execute.c_str());
            int ret_last_profile = execute_profile(context, profile_to_execute, allocation,
                                    cleanup_data, remaining_time);
            if (ret_last_profile != 0) {
                return ret_last_profile;
            }
        }
        return profile->return_code;
    }
    else if (profile->type == ProfileType::DELAY)
    {
        DelayProfileData * data = (DelayProfileData *) profile->data;

        if (data->delay < *remaining_time)
        {
            XBT_INFO("Sleeping the whole task length");
            MSG_process_sleep(data->delay);
            XBT_INFO("Sleeping done");
            *remaining_time = *remaining_time - data->delay;
            return profile->return_code;
        }
        else
        {
            XBT_INFO("Sleeping until walltime");
            MSG_process_sleep(*remaining_time);
            XBT_INFO("Walltime reached");
            *remaining_time = 0;
            return -1;
        }
    }
    else if (profile->type == ProfileType::SMPI)
    {
        SmpiProfileData * data = (SmpiProfileData *) profile->data;
        msg_sem_t sem = MSG_sem_init(1);

        int nb_ranks = data->trace_filenames.size();

        // Let's use the default mapping is none is provided (round-robin on hosts, as we do not
        // know the number of cores on each host)
        if (job->smpi_ranks_to_hosts_mapping.empty())
        {
            job->smpi_ranks_to_hosts_mapping.resize(nb_ranks);
            int host_to_use = 0;
            for (int i = 0; i < nb_ranks; ++i)
            {
                job->smpi_ranks_to_hosts_mapping[i] = host_to_use;
                ++host_to_use %= job->required_nb_res; // ++ is done first
            }
        }

        xbt_assert(nb_ranks == (int) job->smpi_ranks_to_hosts_mapping.size(),
                   "Invalid job %s: SMPI ranks_to_host mapping has an invalid size, as it should "
                   "use %d MPI ranks but the ranking states that there are %d ranks.",
                   job->id.c_str(), nb_ranks, (int) job->smpi_ranks_to_hosts_mapping.size());

        for (int i = 0; i < nb_ranks; ++i)
        {
            char *str_instance_id = NULL;
            int ret = asprintf(&str_instance_id, "%s!%d", job->workload->name.c_str(), job->number);
            (void) ret; // Avoids a warning if assertions are ignored
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

            char *str_rank_id  = NULL;
            ret = asprintf(&str_rank_id, "%d", i);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

            char *str_pname = NULL;
            ret = asprintf(&str_pname, "%d_%d", job->number, i);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

            char **argv = xbt_new(char*, 5);
            argv[0] = xbt_strdup("1"); // Fonction_replay_label (can be ignored, for log only),
            argv[1] = str_instance_id; // Instance Id (application) job_id is used
            argv[2] = str_rank_id;     // Rank Id
            argv[3] = xbt_strdup((char*) data->trace_filenames[i].c_str());
            argv[4] = xbt_strdup("0"); //

            msg_host_t host_to_use = allocation->hosts[job->smpi_ranks_to_hosts_mapping[i]];
            SMPIReplayProcessArguments * message = new SMPIReplayProcessArguments;
            message->semaphore = NULL;
            message->job = job;

            XBT_INFO("Hello!");

            if (i == 0)
            {
                message->semaphore = sem;
            }

            msg_process_t process = MSG_process_create_with_arguments(str_pname, smpi_replay_process,
                                                                      message, host_to_use, 5, argv);
            job->execution_processes.insert(process);

            // todo: avoid memory leaks
            free(str_pname);

        }
        MSG_sem_acquire(sem);
        free(sem);
        return profile->return_code;
    }
    else
        xbt_die("Cannot execute job %s: the profile type '%s' is unknown",
                job->id.c_str(), job->profile.c_str());

    return 1;
}

int delay_job(double sleeptime,
              double * remaining_time)
{
        if (sleeptime < *remaining_time)
        {
            MSG_process_sleep(sleeptime);
            *remaining_time = *remaining_time - sleeptime;
            return 0;
        }
        else
        {
            XBT_INFO("Job has reached walltime");
            MSG_process_sleep(*remaining_time);
            *remaining_time = 0;
            return -1;
        }
}

int execute_job_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    // Retrieving input parameters
    ExecuteJobProcessArguments * args = (ExecuteJobProcessArguments *) MSG_process_get_data(MSG_process_self());

    Workload * workload = args->context->workloads.at(args->allocation->job_id.workload_name);
    Job * job = workload->jobs->at(args->allocation->job_id.job_number);
    job->starting_time = MSG_get_clock();
    job->allocation = args->allocation->machine_ids;
    double remaining_time = (double)job->walltime;

    // If energy is enabled, let us compute the energy used by the machines before running the job
    if (args->context->energy_used)
    {
        job->consumed_energy = consumed_energy_on_machines(args->context, job->allocation);
        // Let's trace the consumed energy
        args->context->energy_tracer.add_job_start(MSG_get_clock(), job->number);
    }

    // Job computation
    args->context->machines.update_machines_on_job_run(job,
                                                       args->allocation->machine_ids,
                                                       args->context);
    CleanExecuteProfileData * cleanup_data = new CleanExecuteProfileData;
    cleanup_data->exec_process_args = args;
    SIMIX_process_on_exit(MSG_process_self(), execute_profile_cleanup, cleanup_data);
    job->return_code = execute_profile(args->context, job->profile, args->allocation, cleanup_data, &remaining_time);
    if (job->return_code == 0)
    {
        XBT_INFO("Job %s finished in time (success)", job->id.c_str());
        job->state = JobState::JOB_STATE_COMPLETED_SUCCESSFULLY;
    }
    else if (job->return_code > 0)
    {
        XBT_INFO("Job %s finished in time (failed)", job->id.c_str());
        job->state = JobState::JOB_STATE_COMPLETED_FAILED;
    }
    else
    {
        XBT_INFO("Job %s had been killed (walltime %g reached)",
                 job->id.c_str(), (double) job->walltime);
        job->state = JobState::JOB_STATE_COMPLETED_KILLED;
        job->kill_reason = "Walltime reached";
        if (args->context->trace_schedule)
        {
            args->context->paje_tracer.add_job_kill(job, args->allocation->machine_ids,
                                                    MSG_get_clock(), true);
        }
    }

    args->context->machines.update_machines_on_job_end(job, args->allocation->machine_ids,
                                                       args->context);
    job->runtime = MSG_get_clock() - job->starting_time;
    if (job->runtime == 0)
    {
        XBT_WARN("Job '%s' computed in null time. Putting epsilon instead.", job->id.c_str());
        job->runtime = Rational(1e-5);
    }

    // If energy is enabled, let us compute the energy used by the machines after running the job
    if (args->context->energy_used)
    {
        long double consumed_energy_before = job->consumed_energy;
        job->consumed_energy = consumed_energy_on_machines(args->context, job->allocation);

        // The consumed energy is the difference (consumed_energy_after_job - consumed_energy_before_job)
        job->consumed_energy -= job->consumed_energy - consumed_energy_before;

        // Let's trace the consumed energy
        args->context->energy_tracer.add_job_end(MSG_get_clock(), job->number);
    }

    if (args->notify_server_at_end)
    {
        // Let us tell the server that the job completed
        JobCompletedMessage * message = new JobCompletedMessage;
        message->job_id = args->allocation->job_id;

        send_message("server", IPMessageType::JOB_COMPLETED, (void*)message);
    }

    job->execution_processes.erase(MSG_process_self());
    return 0;
}

int waiter_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    WaiterProcessArguments * args = (WaiterProcessArguments *) MSG_process_get_data(MSG_process_self());

    double curr_time = MSG_get_clock();

    if (curr_time < args->target_time)
    {
        double time_to_wait = args->target_time - curr_time;
        // Sometimes time_to_wait is so small that it does not affect MSG_process_sleep. The value of 1e-5 have been found on trial-error.
        if(time_to_wait < 1e-5)
        {
            time_to_wait = 1e-5;
        }
        XBT_INFO("Sleeping %g seconds to reach time %g", time_to_wait, args->target_time);
        MSG_process_sleep(time_to_wait);
        XBT_INFO("Sleeping done");
    }
    else
    {
        XBT_INFO("Time %g is already reached, skipping sleep", args->target_time);
    }

    send_message("server", IPMessageType::WAITING_DONE);
    delete args;

    return 0;
}

int execute_profile_cleanup(void * unknown, void * data)
{
    (void) unknown;

    CleanExecuteProfileData * cleanup_data = (CleanExecuteProfileData *) data;
    xbt_assert(cleanup_data != nullptr);

    XBT_DEBUG("before freeing computation amount %p", cleanup_data->computation_amount);
    xbt_free(cleanup_data->computation_amount);
    XBT_DEBUG("before freeing communication amount %p", cleanup_data->communication_amount);
    xbt_free(cleanup_data->communication_amount);

    if (cleanup_data->exec_process_args != nullptr)
    {
        XBT_DEBUG("before deleting exec_process_args->allocation %p",
                  cleanup_data->exec_process_args->allocation);
        delete cleanup_data->exec_process_args->allocation;
        XBT_DEBUG("before deleting exec_process_args %p", cleanup_data->exec_process_args);
        delete cleanup_data->exec_process_args;
    }

    if (cleanup_data->task != nullptr)
    {
        XBT_WARN("Not cleaning the task data to avoid a SG deadlock :(");
        //MSG_task_destroy(cleanup_data->task);
    }

    XBT_DEBUG("before deleting cleanup_data %p", cleanup_data);
    delete cleanup_data;

    return 0;
}

int killer_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    KillerProcessArguments * args = (KillerProcessArguments *) MSG_process_get_data(MSG_process_self());

    for (const JobIdentifier & job_id : args->jobs_ids)
    {
        Job * job = args->context->workloads.job_at(job_id);
        Profile * profile = args->context->workloads.at(job_id.workload_name)->profiles->at(job->profile);
        (void) profile;

        xbt_assert(job->state == JobState::JOB_STATE_RUNNING ||
                   job->state == JobState::JOB_STATE_COMPLETED_KILLED ||
                   job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY ||
                   job->state == JobState::JOB_STATE_COMPLETED_FAILED,
                   "Bad kill: job %s is not running", job->id.c_str());

        if (job->state == JobState::JOB_STATE_RUNNING)
        {
            // Let's kill all the involved processes
            xbt_assert(job->execution_processes.size() > 0);
            for (msg_process_t process : job->execution_processes)
            {
                XBT_INFO("Killing process '%s'", MSG_process_get_name(process));
                MSG_process_kill(process);
            }
            job->execution_processes.clear();

            // Let's update the job information
            job->state = JobState::JOB_STATE_COMPLETED_KILLED;
            job->kill_reason = "Killed from killer_process (probably requested by the decision process)";

            args->context->machines.update_machines_on_job_end(job,
                                                               job->allocation,
                                                               args->context);
            job->runtime = (Rational)MSG_get_clock() - job->starting_time;

            xbt_assert(job->runtime >= 0, "Negative runtime of killed job '%s' (%g)!",
                       job->id.c_str(), (double)job->runtime);
            if (job->runtime == 0)
            {
                XBT_WARN("Killed job '%s' has a null runtime. Putting epsilon instead.",
                         job->id.c_str());
                job->runtime = Rational(1e-5);
            }

            // If energy is enabled, let us compute the energy used by the machines after running the job
            if (args->context->energy_used)
            {
                long double consumed_energy_before = job->consumed_energy;
                job->consumed_energy = consumed_energy_on_machines(args->context, job->allocation);

                // The consumed energy is the difference (consumed_energy_after_job - consumed_energy_before_job)
                job->consumed_energy = job->consumed_energy - consumed_energy_before;

                // Let's trace the consumed energy
                args->context->energy_tracer.add_job_end(MSG_get_clock(), job->number);
            }
        }
    }

    KillingDoneMessage * message = new KillingDoneMessage;
    message->jobs_ids = args->jobs_ids;
    send_message("server", IPMessageType::KILLING_DONE, (void*)message);
    delete args;

    return 0;
}

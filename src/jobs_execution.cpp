/**
 * @file jobs_execution.cpp
 * @brief Contains functions related to the execution of the jobs
 */
#include <regex>

#include "jobs_execution.hpp"
#include "jobs.hpp"
#include "task_execution.hpp"

#include <simgrid/plugins/energy.h>

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

int execute_task(BatTask * btask,
                 BatsimContext *context,
                 const SchedulingAllocation * allocation,
                 CleanExecuteTaskData * cleanup_data,
                 double * remaining_time)
{
    Job * job = btask->parent_job;
    Profile * profile = btask->profile;
    int nb_res = btask->parent_job->required_nb_res;

    // Init task
    btask->parent_job = job;

    if (profile->is_parallel_task())
    {
        int return_code = execute_msg_task(btask, allocation, nb_res, remaining_time,
                                           context, cleanup_data);
        if (return_code != 0)
        {
            return return_code;
        }

        return profile->return_code;
    }
    else if (profile->type == ProfileType::SEQUENCE)
    {
        SequenceProfileData * data = (SequenceProfileData *) profile->data;

        // (Sequences can be repeated several times)
        for (int sequence_iteration = 0; sequence_iteration < data->repeat; sequence_iteration++)
        {
            for (unsigned int profile_index_in_sequence = 0;
                 profile_index_in_sequence < data->sequence.size();
                 profile_index_in_sequence++)
            {
                // Traces how the execution is going so that progress can be retrieved if needed
                btask->current_task_index = sequence_iteration * data->sequence.size() +
                                            profile_index_in_sequence;
                BatTask * sub_btask = new BatTask(job,
                    job->workload->profiles->at(data->sequence[profile_index_in_sequence]));
                btask->sub_tasks.push_back(sub_btask);

                string task_name = "seq" + to_string(job->number) + "'" + job->profile + "'";
                XBT_INFO("Creating sequential tasks '%s'", task_name.c_str());

                int ret_last_profile = execute_task(sub_btask, context,  allocation,
                                                    cleanup_data, remaining_time);

                // The whole sequence fails if a subtask fails
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
        message->job_id = job->id;
        message->message.CopyFrom(data->message, message->message.GetAllocator());

        send_message("server", IPMessageType::FROM_JOB_MSG, (void*)message);

        if (do_delay_task(data->sleeptime, remaining_time) == -1)
        {
            return -1;
        }

        return profile->return_code;
    }
    else if (profile->type == ProfileType::SCHEDULER_RECV)
    {
        SchedulerRecvProfileData * data = (SchedulerRecvProfileData *) profile->data;

        string profile_to_execute = "";
        bool has_messages = false;

        XBT_INFO("Trying to receive message from scheduler");
        if (job->incoming_message_buffer.empty())
        {
            if (data->on_timeout == "")
            {
                XBT_INFO("Waiting for message from scheduler");
                while (true)
                {
                    if (do_delay_task(data->polltime, remaining_time) == -1)
                    {
                        return -1;
                    }

                    if (!job->incoming_message_buffer.empty())
                    {
                        XBT_INFO("Finally got message from scheduler");
                        has_messages = true;
                        break;
                    }
                }
            }
            else
            {
                XBT_INFO("Timeout on waiting for message from scheduler");
                profile_to_execute = data->on_timeout;
            }
        }
        else
        {
            has_messages = true;
        }

        if (has_messages)
        {
            string first_message = job->incoming_message_buffer.front();
            job->incoming_message_buffer.pop_front();

            regex msg_regex(data->regex);
            if (regex_match(first_message, msg_regex))
            {
                XBT_INFO("Message from scheduler matches");
                profile_to_execute = data->on_success;
            }
            else
            {
                XBT_INFO("Message from scheduler does not match");
                profile_to_execute = data->on_failure;
            }
        }

        if (profile_to_execute != "")
        {
            XBT_INFO("Instaciate task from profile: %s", profile_to_execute.c_str());
            int ret_last_profile = execute_task(btask, context, allocation, cleanup_data, remaining_time);
            if (ret_last_profile != 0)
            {
                return ret_last_profile;
            }
        }
        return profile->return_code;
    }
    else if (profile->type == ProfileType::DELAY)
    {
        DelayProfileData * data = (DelayProfileData *) profile->data;

        btask->delay_task_start = MSG_get_clock();
        btask->delay_task_required = data->delay;

        if (do_delay_task(data->delay, remaining_time) == -1)
        {
            return -1;
        }
        return profile->return_code;
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
                   job->id.to_string().c_str(), nb_ranks, (int) job->smpi_ranks_to_hosts_mapping.size());

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
        xbt_die("Cannot execute job %s: the profile '%s' is of unknown type: %s",
                job->id.to_string().c_str(), job->profile.c_str(), profile->json_description.c_str());

    return 1;
}

int do_delay_task(double sleeptime, double * remaining_time)
{
    if (sleeptime < *remaining_time)
    {
        XBT_INFO("Sleeping the whole task length");
        MSG_process_sleep(sleeptime);
        XBT_INFO("Sleeping done");
        *remaining_time = *remaining_time - sleeptime;
        return 0;
    }
    else
    {
        XBT_INFO("Sleeping until walltime");
        MSG_process_sleep(*remaining_time);
        XBT_INFO("Job has reached walltime");
        *remaining_time = 0;
        return -1;
    }
}

/**
 * @brief Hook function given to simgrid to cleanup the task after its
 * execution ends
 * @param unknown unknown
 * @param[in,out] data structure to clean up (cast in * CleanExecuteTaskData)
 * @return always 0
 */
int execute_task_cleanup(void * unknown, void * data)
{
    (void) unknown;

    CleanExecuteTaskData * cleanup_data = (CleanExecuteTaskData *) data;
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
    // Add a cleanup hook on the process
    CleanExecuteTaskData * cleanup_data = new CleanExecuteTaskData;
    cleanup_data->exec_process_args = args;
    SIMIX_process_on_exit(MSG_process_self(), execute_task_cleanup, cleanup_data);

    // Create root task
    job->task = new BatTask(job, workload->profiles->at(job->profile));

    // Execute the process
    job->return_code = execute_task(job->task, args->context, args->allocation,
                                    cleanup_data, &remaining_time);
    if (job->return_code == 0)
    {
        XBT_INFO("Job %s finished in time (success)", job->id.to_string().c_str());
        job->state = JobState::JOB_STATE_COMPLETED_SUCCESSFULLY;
    }
    else if (job->return_code > 0)
    {
        XBT_INFO("Job %s finished in time (failed: return_code=%d)",
                 job->id.to_string().c_str(), job->return_code);
        job->state = JobState::JOB_STATE_COMPLETED_FAILED;
    }
    else
    {
        XBT_INFO("Job %s had been killed (walltime %g reached)",
                 job->id.to_string().c_str(), (double) job->walltime);
        job->state = JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED;
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
        XBT_WARN("Job '%s' computed in null time. Putting epsilon instead.", job->id.to_string().c_str());
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



int killer_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    KillerProcessArguments * args = (KillerProcessArguments *) MSG_process_get_data(MSG_process_self());

    KillingDoneMessage * message = new KillingDoneMessage;
    message->jobs_ids = args->jobs_ids;

    for (const JobIdentifier & job_id : args->jobs_ids)
    {
        Job * job = args->context->workloads.job_at(job_id);
        Profile * profile = args->context->workloads.at(job_id.workload_name)->profiles->at(job->profile);
        (void) profile;

        xbt_assert(! (job->state == JobState::JOB_STATE_REJECTED ||
                      job->state == JobState::JOB_STATE_SUBMITTED ||
                      job->state == JobState::JOB_STATE_NOT_SUBMITTED),
                   "Bad kill: job %s has not been started", job->id.to_string().c_str());

        if (job->state == JobState::JOB_STATE_RUNNING)
        {
            BatTask * job_progress = job->compute_job_progress();

            // Consistency checks
            if (profile->is_parallel_task() ||
                profile->type == ProfileType::DELAY)
            {
                xbt_assert(job_progress != nullptr,
                           "MSG and delay profiles should contain jobs progress");
            }

            // Store job progress in the message
            message->jobs_progress[job_id] = job_progress;

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
                       job->id.to_string().c_str(), (double)job->runtime);
            if (job->runtime == 0)
            {
                XBT_WARN("Killed job '%s' has a null runtime. Putting epsilon instead.",
                         job->id.to_string().c_str());
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

    send_message("server", IPMessageType::KILLING_DONE, (void*)message);
    delete args;

    return 0;
}

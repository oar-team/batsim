/**
 * @file jobs_execution.cpp
 * @brief Contains functions related to the execution of the jobs
 */
#include <regex>

#include "jobs_execution.hpp"
#include "jobs.hpp"
#include "task_execution.hpp"
#include "server.hpp"

#include <simgrid/s4u.hpp>
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

    // args->job->execution_actors.erase(simgrid::s4u::Actor::self()); TODO S4U
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

    // Init task
    btask->parent_job = job;

    if (profile->is_parallel_task())
    {
        int return_code = execute_msg_task(btask, allocation, remaining_time,
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
                BatTask * sub_btask = btask->sub_tasks[profile_index_in_sequence];

                string task_name = "seq" + job->id.to_string() + "'" + job->profile + "'";
                XBT_DEBUG("Creating sequential tasks '%s'", task_name.c_str());

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
            XBT_INFO("Instanciate task from profile: %s", profile_to_execute.c_str());

            btask->current_task_index = 0;
            BatTask * sub_btask = new BatTask(job,
                    job->workload->profiles->at(profile_to_execute));
            btask->sub_tasks.push_back(sub_btask);

            string task_name = "recv" + job->id.to_string() + "'" + job->profile + "'";
            XBT_INFO("Creating receive task '%s'", task_name.c_str());

            int ret_last_profile = execute_task(sub_btask, context, allocation,
                                    cleanup_data, remaining_time);

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
                ++host_to_use %= job->requested_nb_res; // ++ is done first
            }
        }

        xbt_assert(nb_ranks == (int) job->smpi_ranks_to_hosts_mapping.size(),
                   "Invalid job %s: SMPI ranks_to_host mapping has an invalid size, as it should "
                   "use %d MPI ranks but the ranking states that there are %d ranks.",
                   job->id.to_string().c_str(), nb_ranks, (int) job->smpi_ranks_to_hosts_mapping.size());

        for (int i = 0; i < nb_ranks; ++i)
        {
            char *str_instance_id = NULL;
            int ret = asprintf(&str_instance_id, "%s", job->id.to_string().c_str());
            (void) ret; // Avoids a warning if assertions are ignored
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

            char *str_rank_id  = NULL;
            ret = asprintf(&str_rank_id, "%d", i);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

            char *str_pname = NULL;
            ret = asprintf(&str_pname, "%s_%d", job->id.to_string().c_str(), i);
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
            //job->execution_processes.insert(process); TODO S4U
            (void) process;

            // todo: avoid memory leaks
            free(str_pname);

        }
        MSG_sem_acquire(sem);
        MSG_sem_destroy(sem);
        return profile->return_code;
    }
    else
        xbt_die("Cannot execute job %s: the profile '%s' is of unknown type: %s",
                job->id.to_string().c_str(), job->profile.c_str(), profile->json_description.c_str());

    return 1;
}

int do_delay_task(double sleeptime, double * remaining_time)
{
    // if the walltime is not set or not reached
    if (*remaining_time < 0 || sleeptime < *remaining_time)
    {
        XBT_INFO("Sleeping the whole task length");
        MSG_process_sleep(sleeptime);
        XBT_INFO("Sleeping done");
        if (*remaining_time > 0)
        {
            *remaining_time = *remaining_time - sleeptime;
        }
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

    /* TODO S4U
    if (cleanup_data->exec_process_args != nullptr)
    {
        XBT_DEBUG("before deleting exec_process_args->allocation %p",
                  cleanup_data->exec_process_args->allocation);
        delete cleanup_data->exec_process_args->allocation;
        XBT_DEBUG("before deleting exec_process_args %p", cleanup_data->exec_process_args);
        delete cleanup_data->exec_process_args;
    }
    */

    if (cleanup_data->task != nullptr)
    {
        XBT_WARN("Not cleaning the task data to avoid a SG deadlock :(");
        //MSG_task_destroy(cleanup_data->task);
    }

    XBT_DEBUG("before deleting cleanup_data %p", cleanup_data);
    delete cleanup_data;

    return 0;
}

/**
 * @brief Initializes logging structures associated with a task (job execution)
 * @param[in] job The job that is about to be executed
 * @param[in] profile The profile that is about to be executed
 * @param[in] io_profile The IO profile that may also be executed
 * @return The BatTask* associated with the sequential task
 */
BatTask * initialize_sequential_tasks(Job * job, Profile * profile, Profile * io_profile)
{
    BatTask * task = new BatTask(job, profile);

    // leaf of the task tree
    if (profile->type != ProfileType::SEQUENCE)
    {
        task->io_profile = io_profile;
        return task;
    }

    // it's a sequence profile
    SequenceProfileData * data = (SequenceProfileData *) profile->data;
    Profiles * profiles = job->workload->profiles;

    // Sequences can be repeated several times
    for (int repeated = 0; repeated < data->repeat; repeated++)
    {
        for (unsigned int i = 0; i < data->sequence.size(); i++)
        {
            // Get profile from name
            Profile * sub_profile = profiles->at(data->sequence[i]);

            // Manage io profile
            Profile * sub_io_profile = nullptr;
            if (io_profile != nullptr)
            {
                SequenceProfileData * io_data = (SequenceProfileData*)io_profile->data;
                xbt_assert(profiles->exists(io_data->sequence[i]),
                        "The given profile name '%s' does not exists",
                        io_data->sequence[i].c_str());
                sub_io_profile = profiles->at(io_data->sequence[i]);
            }

            // recusrsive call
            BatTask * sub_btask = initialize_sequential_tasks(
                    job, sub_profile, sub_io_profile);

            task->sub_tasks.push_back(sub_btask);
        }
    }

    return task;
}

void execute_job_process(BatsimContext * context,
                         SchedulingAllocation * allocation,
                         bool notify_server_at_end,
                         Profile * io_profile)
{
    Workload * workload = context->workloads.at(allocation->job_id.workload_name);
    Job * job = workload->jobs->at(allocation->job_id);
    Profile * profile = workload->profiles->at(job->profile);

    job->starting_time = MSG_get_clock();
    job->allocation = allocation->machine_ids;
    double remaining_time = (double)job->walltime;

    // Create the root task
    job->task = initialize_sequential_tasks(job, profile, io_profile);
    unsigned int nb_allocated_resources = allocation->machine_ids.size();

    if (allocation->mapping.size() == 0)
    {
        // Default mapping
        allocation->mapping.resize(nb_allocated_resources);
        for (unsigned int i = 0; i < nb_allocated_resources; ++i)
        {
            allocation->mapping[i] = i;
        }
    }

    // generate the hosts used by the job
    allocation->hosts.clear();
    allocation->hosts.reserve(nb_allocated_resources);

    // create hosts list from mapping
    for (unsigned int executor_id = 0; executor_id < allocation->mapping.size(); ++executor_id)
    {
        int machine_id_within_allocated_resources = allocation->mapping[executor_id];
        int machine_id = allocation->machine_ids[machine_id_within_allocated_resources];

        msg_host_t to_add = context->machines[machine_id]->host;
        allocation->hosts.push_back(to_add);
    }

    // Also generate io hosts list if any
    allocation->io_hosts.reserve(allocation->io_allocation.size());
    for (unsigned int id = 0; id < allocation->io_allocation.size(); ++id)
    {
        allocation->io_hosts.push_back(context->machines[id]->host);
    }

    // If energy is enabled, let us compute the energy used by the machines before running the job
    if (context->energy_used)
    {
        job->consumed_energy = consumed_energy_on_machines(context, job->allocation);
        // Let's trace the consumed energy
        context->energy_tracer.add_job_start(MSG_get_clock(), job->id);
    }

    // Job computation
    context->machines.update_machines_on_job_run(job, allocation->machine_ids,
                                                 context);
    // Add a cleanup hook on the process
    CleanExecuteTaskData * cleanup_data = new CleanExecuteTaskData;
    //cleanup_data->exec_process_args = args; TODO S4U
    //MSG_process_on_exit(execute_task_cleanup, cleanup_data); TODO S4U

    // Execute the process
    job->return_code = execute_task(job->task, context, allocation,
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
        if (context->trace_schedule)
        {
            context->paje_tracer.add_job_kill(job, allocation->machine_ids,
                                              MSG_get_clock(), true);
        }
    }

    context->machines.update_machines_on_job_end(job, allocation->machine_ids, context);
    job->runtime = MSG_get_clock() - job->starting_time;
    if (job->runtime == 0)
    {
        XBT_WARN("Job '%s' computed in null time. Putting epsilon instead.", job->id.to_string().c_str());
        job->runtime = Rational(1e-5);
    }

    // If energy is enabled, let us compute the energy used by the machines after running the job
    if (context->energy_used)
    {
        long double consumed_energy_before = job->consumed_energy;
        job->consumed_energy = consumed_energy_on_machines(context, job->allocation);

        // The consumed energy is the difference (consumed_energy_after_job - consumed_energy_before_job)
        job->consumed_energy -= job->consumed_energy - consumed_energy_before;

        // Let's trace the consumed energy
        context->energy_tracer.add_job_end(MSG_get_clock(), job->id);
    }

    if (notify_server_at_end)
    {
        // Let us tell the server that the job completed
        JobCompletedMessage * message = new JobCompletedMessage;
        message->job_id = allocation->job_id;

        send_message("server", IPMessageType::JOB_COMPLETED, (void*)message);
    }

    job->execution_actors.erase(simgrid::s4u::Actor::self());
}

void waiter_process(double target_time, const ServerData * server_data)
{
    double curr_time = MSG_get_clock();

    if (curr_time < target_time)
    {
        double time_to_wait = target_time - curr_time;
        // Sometimes time_to_wait is so small that it does not affect MSG_process_sleep. The value of 1e-5 have been found on trial-error.
        if(time_to_wait < 1e-5)
        {
            time_to_wait = 1e-5;
        }
        XBT_INFO("Sleeping %g seconds to reach time %g", time_to_wait, target_time);
        MSG_process_sleep(time_to_wait);
        XBT_INFO("Sleeping done");
    }
    else
    {
        XBT_INFO("Time %g is already reached, skipping sleep", target_time);
    }

    if (server_data->end_of_simulation_in_send_buffer ||
        server_data->end_of_simulation_sent ||
        server_data->end_of_simulation_ack_received)
    {
        XBT_INFO("Simulation have finished. Thus, NOT sending WAITING_DONE to the server.");
    }
    else
    {
        send_message("server", IPMessageType::WAITING_DONE);
    }
}



void killer_process(BatsimContext * context, std::vector<JobIdentifier> jobs_ids)
{
    KillingDoneMessage * message = new KillingDoneMessage;
    message->jobs_ids = jobs_ids;

    for (const JobIdentifier & job_id : jobs_ids)
    {
        Job * job = context->workloads.job_at(job_id);
        Profile * profile = context->workloads.at(job_id.workload_name)->profiles->at(job->profile);
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
            xbt_assert(job->execution_actors.size() > 0);
            for (simgrid::s4u::ActorPtr actor : job->execution_actors)
            {
                XBT_INFO("Killing process '%s'", actor->get_cname());
                actor->kill();
            }
            job->execution_actors.clear();

            // Let's update the job information
            job->state = JobState::JOB_STATE_COMPLETED_KILLED;

            context->machines.update_machines_on_job_end(job, job->allocation, context);
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
            if (context->energy_used)
            {
                long double consumed_energy_before = job->consumed_energy;
                job->consumed_energy = consumed_energy_on_machines(context, job->allocation);

                // The consumed energy is the difference (consumed_energy_after_job - consumed_energy_before_job)
                job->consumed_energy = job->consumed_energy - consumed_energy_before;

                // Let's trace the consumed energy
                context->energy_tracer.add_job_end(MSG_get_clock(), job->id);
            }
        }
    }

    send_message("server", IPMessageType::KILLING_DONE, (void*)message);
}

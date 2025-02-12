/**
 * @file jobs_execution.cpp
 * @brief Contains functions related to the execution of the jobs
 */
#include <algorithm>
#include <cmath>
#include <regex>

#include "jobs_execution.hpp"
#include "jobs.hpp"
#include "task_execution.hpp"
#include "server.hpp"

#include <simgrid/s4u.hpp>
#include <simgrid/plugins/energy.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(jobs_execution, "jobs_execution"); //!< Logging

using namespace std;

int execute_task(
    BatTask * btask,
    BatsimContext *context,
    const std::shared_ptr<ExecuteJobMessage> & execute_job_msg,
    double * remaining_time)
{
    auto job = static_cast<JobPtr>(btask->parent_job);
    auto profile = btask->profile;

    // Determine which AllocationPlacement to use for this task.
    std::shared_ptr<AllocationPlacement> alloc_placement = execute_job_msg->job_allocation;
    auto alloc_placement_it = execute_job_msg->profile_allocation_override.find(btask->profile->name);
    if (alloc_placement_it != execute_job_msg->profile_allocation_override.end())
    {
        alloc_placement = alloc_placement_it->second;
    }

    // TODO: generate final hosts to use (from allocated hosts, profile data, placement strategy or custom mapping)
    // should be quite simple for profile_type != composition_ptask_merge

    switch(profile->type)
    {
        case ProfileType::PTASK:
        case ProfileType::PTASK_HOMOGENEOUS:
        case ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS:
        case ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES:
        {
            int return_code = execute_parallel_task(btask, alloc_placement, remaining_time, context);
            if (return_code != 0)
            {
                return return_code;
            }

            return profile->return_code;
        }
        case ProfileType::SEQUENTIAL_COMPOSITION:
        {
            auto * data = static_cast<SequenceProfileData *>(profile->data);

            for (unsigned int sequence_iteration = 0; sequence_iteration < data->repetition_count; sequence_iteration++)
            {
                for (unsigned int profile_index_in_sequence = 0;
                    profile_index_in_sequence < data->sequence_names.size();
                    profile_index_in_sequence++)
                {
                    // Trace how the execution is going so that progress can be retrieved if needed
                    btask->current_repetition = sequence_iteration;
                    btask->current_task_index = profile_index_in_sequence;

                    xbt_assert(btask->sub_tasks.empty(), "internal inconsistency: there should be no current sub_tasks");
                    auto sub_profile = data->profile_sequence[profile_index_in_sequence];
                    BatTask * sub_btask = new BatTask(JobPtr(btask->parent_job), sub_profile);
                    btask->sub_tasks.push_back(sub_btask);

                    int ret_last_profile = execute_task(sub_btask, context, execute_job_msg, remaining_time);

                    btask->sub_tasks.clear();

                    // The whole sequence fails if a subtask fails
                    if (ret_last_profile != 0)
                    {
                        return ret_last_profile;
                    }
                }
            }
            return profile->return_code;
        }
        case ProfileType::DELAY:
        {
            auto * data = static_cast<DelayProfileData *>(profile->data);

            btask->delay_task_start = simgrid::s4u::Engine::get_clock();
            btask->delay_task_required = data->delay;

            if (do_delay_task(data->delay, remaining_time) == -1)
            {
                return -1;
            }
            return profile->return_code;
        }
        case ProfileType::FORKJOIN_COMPOSITION:
        {
            xbt_die("Execution of ForkJoin Composition profiles is not implemented yet");
            //TODO implement me
        }
        case ProfileType::PTASK_MERGE_COMPOSITION:
        {
            xbt_die("Execution of Parallel Task Merge Composition profiles is not implemented yet");
            //TODO implement me
        }
        case ProfileType::REPLAY_SMPI:
        case ProfileType::REPLAY_USAGE:
        {
            /*std::vector<std::string> trace_filenames;

            if (profile->type == ProfileType::REPLAY_SMPI)
            {
                auto * data = static_cast<TraceReplayProfileData *>(profile->data);
                trace_filenames = data->trace_filenames;
            }
            else
            {
                auto * data = static_cast<TraceReplayProfileData *>(profile->data);
                trace_filenames = data->trace_filenames;
            }*/

            return execute_trace_replay(btask, alloc_placement, remaining_time, context);
        }
        default:
        {
            xbt_die("Cannot execute job %s: the profile '%s' is of unknown type (%d)",
                    job->id.to_cstring(), job->profile->name.c_str(), (int)profile->type);
        }
    }

    return 1;
}

int do_delay_task(double sleeptime, double * remaining_time)
{
    // if the walltime is not set or not reached
    if (*remaining_time < 0 || sleeptime < *remaining_time)
    {
        XBT_INFO("Sleeping the whole task length");
        simgrid::s4u::this_actor::sleep_for(sleeptime);
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
        simgrid::s4u::this_actor::sleep_for(*remaining_time);
        XBT_INFO("Job has reached walltime");
        *remaining_time = 0;
        return -1;
    }
}

void execute_job_process(
    BatsimContext * context,
    JobPtr job,
    bool notify_server_at_end)
{
    job->starting_time = static_cast<long double>(simgrid::s4u::Engine::get_clock());
    double remaining_time = static_cast<double>(job->walltime);
    const auto & execution_request = job->execution_request;

    // Create the root task
    job->task = new BatTask(job, job->profile);

    if (context->energy_used)
    {
        // If energy is enabled, compute the energy used by the machines before running the job
        job->consumed_energy = consumed_energy_on_machines(context, execution_request->job_allocation->hosts);

        // Trace the consumed energy
        context->energy_tracer.add_job_start(simgrid::s4u::Engine::get_clock(), job->id);
    }

    context->machines.update_machines_on_job_run(job, execution_request->job_allocation->hosts, context);

    // Execute the task
    job->return_code = execute_task(job->task, context, execution_request, &remaining_time);
    if (job->return_code == 0)
    {
        XBT_INFO("Job '%s' finished in time (success)", job->id.to_cstring());
        job->state = JobState::JOB_STATE_COMPLETED_SUCCESSFULLY;
    }
    else if (job->return_code > 0)
    {
        XBT_INFO("Job '%s' finished in time (failed: return_code=%d)", job->id.to_cstring(), job->return_code);
        job->state = JobState::JOB_STATE_COMPLETED_FAILED;
    }
    else if (job->return_code == -1)
    {
        XBT_INFO("Job '%s' had been killed (walltime %Lg reached)", job->id.to_cstring(), job->walltime);
        job->state = JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED;
        if (context->trace_schedule)
        {
            context->paje_tracer.add_job_kill(job->id, execution_request->job_allocation->hosts, simgrid::s4u::Engine::get_clock(), true);
        }
    }
    else if (job->return_code == -2)
    {
        XBT_INFO("Job '%s' has been killed by the scheduler", job->id.to_cstring());
        job->state = JobState::JOB_STATE_COMPLETED_KILLED;
        if (context->trace_schedule)
        {
            context->paje_tracer.add_job_kill(job->id, execution_request->job_allocation->hosts, simgrid::s4u::Engine::get_clock(), true);
        }
    }
    else
    {
        xbt_die("Job '%s' completed with unknown return code: %d", job->id.to_cstring(), job->return_code);
    }

    context->machines.update_machines_on_job_end(job, execution_request->job_allocation->hosts, context);
    job->runtime = static_cast<long double>(simgrid::s4u::Engine::get_clock()) - job->starting_time;
    if (job->runtime == 0)
    {
        XBT_WARN("Job '%s' computed in null time. Putting epsilon instead.", job->id.to_cstring());
        job->runtime = 1e-5l;
    }

    // If energy is enabled, let us compute the energy used by the machines after running the job
    if (context->energy_used)
    {
        // The consumed energy is the difference (consumed_energy_after_job - consumed_energy_before_job)
        long double consumed_energy_before = job->consumed_energy;
        job->consumed_energy = consumed_energy_on_machines(context, execution_request->job_allocation->hosts) - consumed_energy_before;

        // Let's trace the consumed energy
        context->energy_tracer.add_job_end(simgrid::s4u::Engine::get_clock(), job->id);
    }

    if (notify_server_at_end and job->state != JobState::JOB_STATE_COMPLETED_KILLED)
    {
        // The completion of a killed job is already managed in server_on_killing_done

        // Let us tell the server that the job completed
        JobCompletedMessage * message = new JobCompletedMessage;
        message->job = job;

        send_message("server", IPMessageType::JOB_COMPLETED, static_cast<void*>(message));
    }

    job->execution_actors.erase(simgrid::s4u::Actor::self());
}

void oneshot_call_me_later_actor(std::string call_id, double target_time, ServerData * server_data)
{
    double curr_time = simgrid::s4u::Engine::get_clock();

    if (curr_time < target_time)
    {
        double time_to_wait = target_time - curr_time;
        // Sometimes time_to_wait is so small that it does not affect simgrid::s4u::this_actor::sleep_for. The value of 1e-5 have been found on trial-error.
        if(time_to_wait < 1e-5)
        {
            time_to_wait = 1e-5;
        }
        XBT_INFO("Sleeping %g seconds to reach time %g", time_to_wait, target_time);
        simgrid::s4u::this_actor::sleep_for(time_to_wait);
        XBT_INFO("Sleeping done");
    }
    else
    {
        XBT_INFO("Time %g is already reached, skipping sleep", target_time);
    }

    if (server_data->end_of_simulation_sent ||
        server_data->end_of_simulation_ack_received)
    {
        XBT_INFO("Simulation have finished. Thus, NOT sending ONESHOT_REQUESTED_CALL to the server.");
    }
    else
    {
        auto * msg = new OneShotRequestedCallMessage;
        msg->call.call_id = call_id;
        msg->call.is_last_periodic_call = false;
        send_message("server", IPMessageType::ONESHOT_REQUESTED_CALL, msg);
    }
}

bool cancel_ptasks(BatTask * btask)
{
    bool cancelled = false;
    if (btask->ptask != nullptr)
    {
        XBT_DEBUG("Cancelling ptask for job '%s' with profile '%s'", static_cast<JobPtr>(btask->parent_job)->id.to_cstring(), btask->profile->name.c_str());
        btask->ptask->cancel();
        cancelled = true;
    }

    // If this is a composed task, cancel sub_tasks recursively
    if (!btask->sub_tasks.empty())
    {
        for (auto * sub_task : btask->sub_tasks)
        {
            bool newly_cancelled = cancel_ptasks(sub_task);
            cancelled = cancelled || newly_cancelled;
        }
    }

    return cancelled;
}


void killer_process(
    BatsimContext * context,
    KillJobsMessage * kill_jobs_msg,
    JobState killed_job_state,
    bool acknowledge_kill_on_protocol)
{
    KillingDoneMessage * message = new KillingDoneMessage;
    message->kill_jobs_message = kill_jobs_msg;
    message->acknowledge_kill_on_protocol = acknowledge_kill_on_protocol;

    for (auto job_id : kill_jobs_msg->job_ids)
    {
        auto job = context->workloads.job_at(job_id);
        xbt_assert(! (job->state == JobState::JOB_STATE_REJECTED ||
                      job->state == JobState::JOB_STATE_SUBMITTED ||
                      job->state == JobState::JOB_STATE_NOT_SUBMITTED),
            "Invalid kill: job %s has not been started yet (state='%s')",
            job_id.to_cstring(), job_state_to_string(job->state).c_str()
        );

        if (job->state == JobState::JOB_STATE_RUNNING)
        {
            auto task = job->task;
            xbt_assert(task != nullptr, "Internal error");

            // Compute and store the kill progress of this job in the message
            auto kill_progress = protocol::battask_to_kill_progress(task);
            message->jobs_progress[job_id.to_string()] = kill_progress;

            // Store the job profile if required
            if (context->forward_profiles_on_jobs_killed)
            {
                const std::string profile_id = job->profile->workload->name + '!' + job->profile->name;
                message->profiles[profile_id] = protocol::to_profile(*(job->profile));
            }

            // Try to cancel the parallel task executors if they exist
            bool cancelled_ptask = cancel_ptasks(job->task);

            if (!cancelled_ptask)
            {
                // There was no ptask running, directly kill the actors

                // Kill all the involved processes
                xbt_assert(job->execution_actors.size() > 0, "kill inconsistency: no actors to kill while job's task could not be cancelled");
                for (simgrid::s4u::ActorPtr actor : job->execution_actors)
                {
                    XBT_INFO("Killing process '%s'", actor->get_cname());
                    actor->kill();
                }
                job->execution_actors.clear();

                // Update the job information
                job->state = killed_job_state;

                context->machines.update_machines_on_job_end(job, job->execution_request->job_allocation->hosts, context);
                job->runtime = static_cast<long double>(simgrid::s4u::Engine::get_clock()) - job->starting_time;

                xbt_assert(job->runtime >= 0, "Negative runtime of killed job '%s' (%Lg)!", job_id.to_cstring(), job->runtime);
                if (job->runtime == 0)
                {
                    XBT_WARN("Killed job '%s' has a null runtime. Putting epsilon instead.",
                             job_id.to_cstring());
                    job->runtime = 1e-5l;
                }

                // If energy is enabled, let us compute the energy used by the machines after running the job
                if (context->energy_used)
                {
                    long double consumed_energy_before = job->consumed_energy;
                    job->consumed_energy = consumed_energy_on_machines(context, job->execution_request->job_allocation->hosts);

                    // The consumed energy is the difference (consumed_energy_after_job - consumed_energy_before_job)
                    job->consumed_energy = job->consumed_energy - consumed_energy_before;

                    // Trace the consumed energy
                    context->energy_tracer.add_job_end(simgrid::s4u::Engine::get_clock(), job_id);
                }
            }
            // Else the running ptask was asked to cancel by itself.
            // The job process will regularly terminate with the status JOB_STATE_COMPLETED_KILLED
        }
    }

    send_message("server", IPMessageType::KILLING_DONE, static_cast<void*>(message));
}

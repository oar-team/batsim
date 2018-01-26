/**
 * @file server.cpp
 * @brief Contains functions related to the general orchestration of the simulation
 */

#include "server.hpp"

#include <string>

#include <boost/algorithm/string.hpp>

#include <simgrid/msg.h>

#include "context.hpp"
#include "ipp.hpp"
#include "network.hpp"
#include "jobs_execution.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(server, "server"); //!< Logging

using namespace std;

int server_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ServerProcessArguments * args = (ServerProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;

    ServerData * data = new ServerData;
    data->context = context;

    // Let's tell the Decision process that the simulation is about to begin
    // (and that some data can be read from the data storage)
    context->proto_writer->append_simulation_begins(context->machines,
                                                    context->workloads,
                                                    context->config_file,
                                                    context->allow_time_sharing,
                                                    MSG_get_clock());

    RequestReplyProcessArguments * req_rep_args = new RequestReplyProcessArguments;
    req_rep_args->context = context;
    req_rep_args->send_buffer = context->proto_writer->generate_current_message(MSG_get_clock());
    context->proto_writer->clear();

    MSG_process_create("Scheduler REQ-REP", request_reply_scheduler_process,
                       (void*)req_rep_args, MSG_host_self());
    data->sched_ready = false;

    // Let's prepare a handler map to react on events
    std::map<IPMessageType, std::function<void(ServerData *, IPMessage *)>> handler_map;
    handler_map[IPMessageType::JOB_SUBMITTED] = server_on_job_submitted;
    handler_map[IPMessageType::JOB_SUBMITTED_BY_DP] = server_on_submit_job;
    handler_map[IPMessageType::PROFILE_SUBMITTED_BY_DP] = server_on_submit_profile;
    handler_map[IPMessageType::JOB_COMPLETED] = server_on_job_completed;
    handler_map[IPMessageType::PSTATE_MODIFICATION] = server_on_pstate_modification;
    handler_map[IPMessageType::SCHED_EXECUTE_JOB] = server_on_execute_job;
    handler_map[IPMessageType::SCHED_CHANGE_JOB_STATE] = server_on_change_job_state;
    handler_map[IPMessageType::TO_JOB_MSG] = server_on_to_job_msg;
    handler_map[IPMessageType::FROM_JOB_MSG] = server_on_from_job_msg;
    handler_map[IPMessageType::SCHED_REJECT_JOB] = server_on_reject_job;
    handler_map[IPMessageType::SCHED_KILL_JOB] = server_on_kill_jobs;
    handler_map[IPMessageType::SCHED_CALL_ME_LATER] = server_on_call_me_later;
    handler_map[IPMessageType::SCHED_TELL_ME_ENERGY] = server_on_sched_tell_me_energy;
    handler_map[IPMessageType::SCHED_WAIT_ANSWER] = server_on_sched_wait_answer;
    handler_map[IPMessageType::WAIT_QUERY] = server_on_wait_query;
    handler_map[IPMessageType::SCHED_READY] = server_on_sched_ready;
    handler_map[IPMessageType::WAITING_DONE] = server_on_waiting_done;
    handler_map[IPMessageType::KILLING_DONE] = server_on_killing_done;
    handler_map[IPMessageType::SUBMITTER_HELLO] = server_on_submitter_hello;
    handler_map[IPMessageType::SUBMITTER_BYE] = server_on_submitter_bye;
    handler_map[IPMessageType::SWITCHED_ON] = server_on_switched;
    handler_map[IPMessageType::SWITCHED_OFF] = server_on_switched;
    handler_map[IPMessageType::END_DYNAMIC_SUBMIT] = server_on_end_dynamic_submit;
    handler_map[IPMessageType::CONTINUE_DYNAMIC_SUBMIT] = server_on_continue_dynamic_submit;

    // Simulation loop
    while ((data->nb_submitters == 0 && !context->submission_sched_enabled) || // If dynamic submissions are not enabled: wait for the first submitter
           (data->nb_submitters_finished < data->nb_submitters) || // All submitters must have finished
           (data->nb_completed_jobs < data->nb_submitted_jobs) || // All jobs must have finished
           (!data->sched_ready) || // A scheduler answer is being injected into the simulation
           (data->nb_switching_machines > 0) || // Some machines are switching state
           (data->nb_waiters > 0) || // The scheduler requested to be called in the future
           (data->nb_killers > 0) || // Some jobs are being killed
           (context->submission_sched_enabled && // If dynamic job submission are enabled
            !context->submission_sched_finished)) // The end of submissions must have been received
    {
        // Let's wait a message from a node or the request-reply process...
        msg_task_t task_received = NULL;
        IPMessage * task_data;
        MSG_task_receive(&(task_received), "server");

        // Let's handle the message
        task_data = (IPMessage *) MSG_task_get_data(task_received);
        XBT_INFO("Server received a message of type %s:",
                 ip_message_type_to_string(task_data->type).c_str());

        xbt_assert(handler_map.count(task_data->type) == 1,
                   "The server does not know how to handle message type %s.",
                   ip_message_type_to_string(task_data->type).c_str());
        auto handler_function = handler_map[task_data->type];
        handler_function(data, task_data);

        // Let's delete the message data
        delete task_data;
        MSG_task_destroy(task_received);

        // Let's send a message to the scheduler if needed
        if (data->sched_ready && // The scheduler must be ready
            !data->end_of_simulation_sent && // It will NOT be called if SIMULATION_ENDS has already been sent
            !context->proto_writer->is_empty()) // There must be something to send to it
        {
            RequestReplyProcessArguments * req_rep_args = new RequestReplyProcessArguments;
            req_rep_args->context = context;
            req_rep_args->send_buffer = context->proto_writer->generate_current_message(MSG_get_clock());
            context->proto_writer->clear();

            MSG_process_create("Scheduler REQ-REP", request_reply_scheduler_process, (void*)req_rep_args, MSG_host_self());
            data->sched_ready = false;
            if (data->all_jobs_submitted_and_completed)
            {
                data->end_of_simulation_sent = true;
            }
        }

    } // end of while

    XBT_INFO("Simulation is finished!");
    bool simulation_is_completed = data->all_jobs_submitted_and_completed;
    (void) simulation_is_completed; // Avoids a warning if assertions are ignored
    xbt_assert(simulation_is_completed, "Left simulation loop, but the simulation does NOT seem finished...");

    delete data;
    delete args;
    return 0;
}

void server_on_submitter_hello(ServerData * data,
                               IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    xbt_assert(!data->all_jobs_submitted_and_completed,
               "A new submitter said hello but all jobs have already been submitted and completed... Aborting.");
    SubmitterHelloMessage * message = (SubmitterHelloMessage *) task_data->data;

    xbt_assert(data->submitters.count(message->submitter_name) == 0,
               "Invalid new submitter '%s': a submitter with the same name already exists!",
               message->submitter_name.c_str());

    data->nb_submitters++;

    ServerData::Submitter * submitter = new ServerData::Submitter;
    submitter->mailbox = message->submitter_name;
    submitter->should_be_called_back = message->enable_callback_on_job_completion;

    data->submitters[message->submitter_name] = submitter;

    XBT_INFO("New submitter said hello. Number of polite submitters: %d",
             data->nb_submitters);
}

void server_on_submitter_bye(ServerData * data,
                             IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    SubmitterByeMessage * message = (SubmitterByeMessage *) task_data->data;

    xbt_assert(data->submitters.count(message->submitter_name) == 1);
    delete data->submitters[message->submitter_name];
    data->submitters.erase(message->submitter_name);

    data->nb_submitters_finished++;
    if (message->is_workflow_submitter)
    {
        data->nb_workflow_submitters_finished++;
    }
    XBT_INFO("A submitted said goodbye. Number of finished submitters: %d",
             data->nb_submitters_finished);

    check_submitted_and_completed(data);
}

void server_on_job_completed(ServerData * data,
                             IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    JobCompletedMessage * message = (JobCompletedMessage *) task_data->data;

    if (data->origin_of_jobs.count(message->job_id) == 1)
    {
        // Let's call the submitter which submitted the job back
        SubmitterJobCompletionCallbackMessage * msg = new SubmitterJobCompletionCallbackMessage;
        msg->job_id = message->job_id;

        ServerData::Submitter * submitter = data->origin_of_jobs.at(message->job_id);
        dsend_message(submitter->mailbox, IPMessageType::SUBMITTER_CALLBACK, (void*) msg);

        data->origin_of_jobs.erase(message->job_id);
    }

    data->nb_running_jobs--;
    xbt_assert(data->nb_running_jobs >= 0);
    data->nb_completed_jobs++;
    xbt_assert(data->nb_completed_jobs + data->nb_running_jobs <= data->nb_submitted_jobs);
    Job * job = data->context->workloads.job_at(message->job_id);

    XBT_INFO("Job %s!%d COMPLETED. %d jobs completed so far",
             job->workload->name.c_str(), job->number, data->nb_completed_jobs);

    string status = "UNKNOWN";
    if (job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY)
    {
        status = "SUCCESS";
    }
    else if (job->state == JobState::JOB_STATE_COMPLETED_FAILED)
    {
        status = "FAILED";
    }
    else if (job->state == JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED)
    {
        status = "TIMEOUT";
    }

    data->context->proto_writer->append_job_completed(message->job_id.to_string(),
                                                      status,
                                                      job_state_to_string(job->state),
                                                      job->kill_reason,
                                                      job->allocation.to_string_hyphen(" "),
                                                      job->return_code,
                                                      MSG_get_clock());

    check_submitted_and_completed(data);
}

void server_on_job_submitted(ServerData * data,
                             IPMessage * task_data)
{
    // Ignore all submissions if -k was specified and all workflows have completed
    if ((data->context->workflows.size() != 0) && (data->context->terminate_with_last_workflow) &&
        (data->nb_workflow_submitters_finished == data->context->workflows.size()))
    {
        XBT_INFO("Ignoring Job due to -k command-line option");
        return;
    }

    xbt_assert(task_data->data != nullptr);
    JobSubmittedMessage * message = (JobSubmittedMessage *) task_data->data;

    xbt_assert(data->submitters.count(message->submitter_name) == 1);

    ServerData::Submitter * submitter = data->submitters.at(message->submitter_name);
    if (submitter->should_be_called_back)
    {
        xbt_assert(data->origin_of_jobs.count(message->job_id) == 0);
        data->origin_of_jobs[message->job_id] = submitter;
    }

    // Let's retrieve the Job from memory (or add it into memory if it is dynamic)
    XBT_INFO("GOT JOB: %s %d\n", message->job_id.workload_name.c_str(), message->job_id.job_number);
    xbt_assert(data->context->workloads.job_exists(message->job_id));
    Job * job = data->context->workloads.job_at(message->job_id);
    job->id = message->job_id;

    // Update control information
    job->state = JobState::JOB_STATE_SUBMITTED;
    ++data->nb_submitted_jobs;
    XBT_INFO("Job %s SUBMITTED. %d jobs submitted so far",
             message->job_id.to_string().c_str(), data->nb_submitted_jobs);

    string job_json_description, profile_json_description;

    if (!data->context->redis_enabled)
    {
        job_json_description = job->json_description;
        if (data->context->submission_forward_profiles)
        {
            profile_json_description = job->workload->profiles->at(job->profile)->json_description;
        }
    }

    data->context->proto_writer->append_job_submitted(job->id.to_string(), job_json_description,
                                                      profile_json_description, MSG_get_clock());
}

void server_on_pstate_modification(ServerData * data,
                                   IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    PStateModificationMessage * message = (PStateModificationMessage *) task_data->data;

    data->context->current_switches.add_switch(message->machine_ids, message->new_pstate);

    if (data->context->energy_used)
    {
        data->context->energy_tracer.add_pstate_change(MSG_get_clock(), message->machine_ids,
                                                       message->new_pstate);
    }

    // Let's quickly check whether this is a switchON or a switchOFF
    // Unknown transition states will be set to -42.
    int transition_state = -42;
    Machine * first_machine = data->context->machines[message->machine_ids.first_element()];
    if (first_machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE)
    {
        transition_state = -1; // means we are switching to a COMPUTATION_PSTATE
    }
    else if (first_machine->pstates[message->new_pstate] == PStateType::SLEEP_PSTATE)
    {
        transition_state = -2; // means we are switching to a SLEEP_PSTATE
    }

    if (data->context->energy_used)
    {
        // The pstate is set to an invalid one to know the machines are in transition.
        data->context->pstate_tracer.add_pstate_change(MSG_get_clock(), message->machine_ids,
                                                       transition_state);
    }

    // Let's mark that some switches have been requested
    data->context->nb_grouped_switches++;
    data->context->nb_machine_switches += message->machine_ids.size();

    for (auto machine_it = message->machine_ids.elements_begin();
         machine_it != message->machine_ids.elements_end();
         ++machine_it)
    {
        const int machine_id = *machine_it;
        Machine * machine = data->context->machines[machine_id];
        int curr_pstate = MSG_host_get_pstate(machine->host);

        if (machine->pstates[curr_pstate] == PStateType::COMPUTATION_PSTATE)
        {
            if (machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE)
            {
                XBT_INFO("Switching machine %d ('%s') pstate : %d -> %d.", machine->id,
                         machine->name.c_str(), curr_pstate, message->new_pstate);
                MSG_host_set_pstate(machine->host, message->new_pstate);
                xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

                MachineRange all_switched_machines;
                if (data->context->current_switches.mark_switch_as_done(machine->id, message->new_pstate,
                                                                        all_switched_machines,
                                                                        data->context))
                {
                    data->context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                               std::to_string(message->new_pstate),
                                                                               MSG_get_clock());
                }
            }
            else if (machine->pstates[message->new_pstate] == PStateType::SLEEP_PSTATE)
            {
                machine->update_machine_state(MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING);
                SwitchPStateProcessArguments * args = new SwitchPStateProcessArguments;
                args->context = data->context;
                args->machine_id = machine_id;
                args->new_pstate = message->new_pstate;

                string pname = "switch ON " + to_string(machine_id);
                MSG_process_create(pname.c_str(), switch_off_machine_process, (void*)args, machine->host);

                ++data->nb_switching_machines;
            }
            else
            {
                XBT_ERROR("Switching from a communication pstate to an invalid pstate on machine %d ('%s') : %d -> %d",
                          machine->id, machine->name.c_str(), curr_pstate, message->new_pstate);
            }
        }
        else if (machine->pstates[curr_pstate] == PStateType::SLEEP_PSTATE)
        {
            xbt_assert(machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE,
                    "Switching from a sleep pstate to a non-computation pstate on machine %d ('%s') : %d -> %d, which is forbidden",
                    machine->id, machine->name.c_str(), curr_pstate, message->new_pstate);

            machine->update_machine_state(MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING);
            SwitchPStateProcessArguments * args = new SwitchPStateProcessArguments;
            args->context = data->context;
            args->machine_id = machine_id;
            args->new_pstate = message->new_pstate;

            string pname = "switch OFF " + to_string(machine_id);
            MSG_process_create(pname.c_str(), switch_on_machine_process, (void*)args, machine->host);

            ++data->nb_switching_machines;
        }
        else
        {
            XBT_ERROR("Machine %d ('%s') has an invalid pstate : %d", machine->id, machine->name.c_str(), curr_pstate);
        }
    }

    if (data->context->trace_machine_states)
    {
        data->context->machine_state_tracer.write_machine_states(MSG_get_clock());
    }
}

void server_on_waiting_done(ServerData * data,
                            IPMessage * task_data)
{
    (void) task_data;
    data->context->proto_writer->append_requested_call(MSG_get_clock());
    --data->nb_waiters;
}

void server_on_sched_ready(ServerData * data,
                           IPMessage * task_data)
{
    (void) task_data;
    data->sched_ready = true;
}

void server_on_sched_wait_answer(ServerData * data,
                                 IPMessage * task_data)
{
    (void) data;
    SchedWaitAnswerMessage * message = new SchedWaitAnswerMessage;
    *message = *( (SchedWaitAnswerMessage *) task_data->data);

    //    Submitter * submitter = origin_of_wait_queries.at({message->nb_resources,message->processing_time});
    dsend_message(message->submitter_name, IPMessageType::SCHED_WAIT_ANSWER, (void*) message);
    //    origin_of_wait_queries.erase({message->nb_resources,message->processing_time});
}

void server_on_sched_tell_me_energy(ServerData * data,
                                    IPMessage * task_data)
{
    (void) task_data;
    long double total_consumed_energy = data->context->machines.total_consumed_energy(data->context);
    data->context->proto_writer->append_answer_energy(total_consumed_energy, MSG_get_clock());
}

void server_on_wait_query(ServerData * data,
                          IPMessage * task_data)
{
    (void) data;
    (void) task_data;
    //WaitQueryMessage * message = (WaitQueryMessage *) task_data->data;

    //    XBT_INFO("received : %s , %s\n", to_string(message->nb_resources).c_str(), to_string(message->processing_time).c_str());
    xbt_assert(false, "Unimplemented! TODO");

    //Submitter * submitter = submitters.at(message->submitter_name);
    //origin_of_wait_queries[{message->nb_resources,message->processing_time}] = submitter;
}

void server_on_switched(ServerData * data,
                        IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    SwitchMessage * message = (SwitchMessage *) task_data->data;

    xbt_assert(data->context->machines.exists(message->machine_id));
    Machine * machine = data->context->machines[message->machine_id];
    (void) machine; // Avoids a warning if assertions are ignored
    xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

    MachineRange all_switched_machines;
    if (data->context->current_switches.mark_switch_as_done(message->machine_id, message->new_pstate,
                                                            all_switched_machines, data->context))
    {
        if (data->context->trace_machine_states)
        {
            data->context->machine_state_tracer.write_machine_states(MSG_get_clock());
        }

        data->context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                   std::to_string(message->new_pstate),
                                                                   MSG_get_clock());
    }

    --data->nb_switching_machines;
}

void server_on_killing_done(ServerData * data,
                            IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    KillingDoneMessage * message = (KillingDoneMessage *) task_data->data;

    vector<string> job_ids_str;
    vector<string> really_killed_job_ids_str;
    job_ids_str.reserve(message->jobs_ids.size());
    map<string, BatTask *> jobs_progress_str;

    // manage job Id list
    for (const JobIdentifier & job_id : message->jobs_ids)
    {
        job_ids_str.push_back(job_id.to_string());

        // store job progress from BatTask tree in str
        jobs_progress_str[job_id.to_string()] = message->jobs_progress[job_id];

        const Job * job = data->context->workloads.job_at(job_id);
        if (job->state == JobState::JOB_STATE_COMPLETED_KILLED)
        {
            data->nb_running_jobs--;
            xbt_assert(data->nb_running_jobs >= 0);
            data->nb_completed_jobs++;
            xbt_assert(data->nb_completed_jobs + data->nb_running_jobs <= data->nb_submitted_jobs);

            really_killed_job_ids_str.push_back(job_id.to_string());

        }
    }

    XBT_INFO("Jobs {%s} have been killed (the following ones have REALLY been killed: {%s})",
             boost::algorithm::join(job_ids_str, ",").c_str(),
             boost::algorithm::join(really_killed_job_ids_str, ",").c_str());

    data->context->proto_writer->append_job_killed(job_ids_str, jobs_progress_str, MSG_get_clock());
    --data->nb_killers;

    check_submitted_and_completed(data);
}

void server_on_end_dynamic_submit(ServerData * data,
                                  IPMessage * task_data)
{
    (void) task_data;
    data->context->submission_sched_finished = true;

    check_submitted_and_completed(data);
}

void server_on_continue_dynamic_submit(ServerData * data,
                                  IPMessage * task_data)
{
    (void) task_data;
    data->context->submission_sched_finished = false;

    check_submitted_and_completed(data);
}

void server_on_submit_job(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    JobSubmittedByDPMessage * message = (JobSubmittedByDPMessage *) task_data->data;

    // Let's update global states
    ++data->nb_submitted_jobs;

    const Job * job = data->context->workloads.job_at(message->job_id);

    if (data->context->submission_sched_ack)
    {
        string job_json_description, profile_json_description;

        if (!data->context->redis_enabled)
        {
            job_json_description = job->json_description;
            if (data->context->submission_forward_profiles)
            {
                profile_json_description = job->workload->profiles->at(job->profile)->json_description;
            }
        }

        data->context->proto_writer->append_job_submitted(job->id.to_string(), job_json_description,
                                                          profile_json_description,
                                                          MSG_get_clock());
    }
}

void server_on_submit_profile(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    ProfileSubmittedByDPMessage * message = (ProfileSubmittedByDPMessage *) task_data->data;

    xbt_assert(data->context->submission_sched_enabled,
               "Profile submission coming from the decision process received but the option "
               "seems disabled... It can be activated by specifying a configuration file "
               "to Batsim.");

    // Let's create the workload if it doesn't exist, or retrieve it otherwise
    Workload * workload = nullptr;
    if (data->context->workloads.exists(message->workload_name))
    {
        workload = data->context->workloads.at(message->workload_name);
    }
    else
    {
        workload = new Workload(message->workload_name, "Dynamic");
        data->context->workloads.insert_workload(workload->name, workload);
    }

    if (!workload->profiles->exists(message->profile_name))
    {
        XBT_INFO("Adding user-submitted profile %s to workload %s",
                message->profile_name.c_str(),
                message->workload_name.c_str());
        Profile * profile = Profile::from_json(message->profile_name,
                                               message->profile,
                                               "Invalid JSON profile received from the scheduler");
        workload->profiles->add_profile(message->profile_name, profile);
    }
}

void server_on_change_job_state(ServerData * data,
                                IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    ChangeJobStateMessage * message = (ChangeJobStateMessage *) task_data->data;

    Job * job = data->context->workloads.job_at(message->job_id);

    XBT_INFO("Change job state: Job %d (workload=%s) to state %s (kill_Reason=%s)",
             job->number, job->workload->name.c_str(),
             message->job_state.c_str(), message->kill_reason.c_str());

    JobState new_state = job_state_from_string(message->job_state);

    switch (job->state)
    {
    case JobState::JOB_STATE_SUBMITTED:
        switch (new_state)
        {
        case JobState::JOB_STATE_RUNNING:
            job->starting_time = MSG_get_clock();
            data->nb_running_jobs++;
            xbt_assert(data->nb_running_jobs <= data->nb_submitted_jobs);
            break;
        case JobState::JOB_STATE_REJECTED:
            data->nb_completed_jobs++;
            xbt_assert(data->nb_completed_jobs + data->nb_running_jobs <= data->nb_submitted_jobs);
            break;
        default:
            xbt_assert(false,
                       "Can only change the state of a submitted job to running or rejected. "
                       "State was %s", job_state_to_string(job->state).c_str());
        }
        break;
    case JobState::JOB_STATE_RUNNING:
        switch (new_state)
        {
        case JobState::JOB_STATE_COMPLETED_SUCCESSFULLY:
        case JobState::JOB_STATE_COMPLETED_FAILED:
        case JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED:
        case JobState::JOB_STATE_COMPLETED_KILLED:
            job->runtime = MSG_get_clock() - job->starting_time;
            data->nb_running_jobs--;
            xbt_assert(data->nb_running_jobs >= 0);
            data->nb_completed_jobs++;
            xbt_assert(data->nb_completed_jobs + data->nb_running_jobs <= data->nb_submitted_jobs);
            break;
        default:
            xbt_assert(false,
                       "Can only change the state of a running job to completed "
                       "(successfully, failed, and killed). State was %s",
                       job_state_to_string(job->state).c_str());
        }
        break;
    default:
        xbt_assert(false,
                   "Can only change the state of a submitted or running job. State was %s",
                   job_state_to_string(job->state).c_str());
    }

    job->state = new_state;
    job->kill_reason = message->kill_reason;

    XBT_INFO("Job state changed: Job %d (workload=%s)",
             job->number, job->workload->name.c_str());

    check_submitted_and_completed(data);
}

void server_on_to_job_msg(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    ToJobMessage * message = (ToJobMessage *) task_data->data;

    Job * job = data->context->workloads.job_at(message->job_id);

    XBT_INFO("Send message to job: Job %d (workload=%s) message=%s",
             job->number, job->workload->name.c_str(),
             message->message.c_str());

    job->incoming_message_buffer.push_back(message->message);

    check_submitted_and_completed(data);
}

void server_on_from_job_msg(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    FromJobMessage * message = (FromJobMessage *) task_data->data;

    Job * job = data->context->workloads.job_at(message->job_id);

    XBT_INFO("Send message to scheduler: Job %d (workload=%s)",
             job->number, job->workload->name.c_str());

    data->context->proto_writer->append_from_job_message(message->job_id.to_string(),
                                                         message->message,
                                                         MSG_get_clock());

    check_submitted_and_completed(data);
}

void server_on_reject_job(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    JobRejectedMessage * message = (JobRejectedMessage *) task_data->data;

    Job * job = data->context->workloads.job_at(message->job_id);
    job->state = JobState::JOB_STATE_REJECTED;
    data->nb_completed_jobs++;

    XBT_INFO("Job %d (workload=%s) has been rejected",
             job->number, job->workload->name.c_str());

    check_submitted_and_completed(data);
}

void server_on_kill_jobs(ServerData * data,
                         IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    KillJobMessage * message = (KillJobMessage *) task_data->data;

    KillerProcessArguments * args = new KillerProcessArguments;
    args->context = data->context;

    for (const JobIdentifier & job_id : message->jobs_ids)
    {
        Job * job = data->context->workloads.job_at(job_id);

        // Let's discard jobs whose kill has already been requested
        if (!job->kill_requested)
        {
            // Let's check the job state
            xbt_assert(job->state == JobState::JOB_STATE_RUNNING || job->is_complete(),
                       "Invalid KILL_JOB: job_id '%s' refers to a job not being executed nor completed.",
                       job_id.to_string().c_str());

            // Let's mark that the job kill has been requested
            job->kill_requested = true;

            // The job is included in the killer_process arguments
            args->jobs_ids.push_back(job_id);
        }
    }

    MSG_process_create("killer_process", killer_process, (void *) args, MSG_host_self());
    ++data->nb_killers;
}

void server_on_call_me_later(ServerData * data,
                             IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    CallMeLaterMessage * message = (CallMeLaterMessage *) task_data->data;

    xbt_assert(message->target_time > MSG_get_clock(),
               "You asked to be awaken in the past! (you ask: %f, it is: %f)",
               message->target_time, MSG_get_clock());

    WaiterProcessArguments * args = new WaiterProcessArguments;
    args->target_time = message->target_time;

    string pname = "waiter " + to_string(message->target_time);
    MSG_process_create(pname.c_str(), waiter_process, (void*) args,
                       data->context->machines.master_machine()->host);
    ++data->nb_waiters;
}

void server_on_execute_job(ServerData * data,
                           IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr);
    ExecuteJobMessage * message = (ExecuteJobMessage *) task_data->data;
    SchedulingAllocation * allocation = message->allocation;

    xbt_assert(data->context->workloads.job_exists(allocation->job_id),
               "Trying to execute job '%s', which does NOT exist!",
               allocation->job_id.to_string().c_str());

    Job * job = data->context->workloads.job_at(allocation->job_id);
    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
               "Cannot execute job '%s': its state (%d) is not JOB_STATE_SUBMITTED.",
               job->id.to_string().c_str(), job->state);

    job->state = JobState::JOB_STATE_RUNNING;

    data->nb_running_jobs++;
    xbt_assert(data->nb_running_jobs <= data->nb_submitted_jobs);

    if (!data->context->allow_time_sharing)
    {
        for (auto machine_id_it = allocation->machine_ids.elements_begin(); machine_id_it != allocation->machine_ids.elements_end(); ++machine_id_it)
        {
            int machine_id = *machine_id_it;
            const Machine * machine = data->context->machines[machine_id];
            (void) machine; // Avoids a warning if assertions are ignored
            xbt_assert(machine->jobs_being_computed.empty(),
                       "Invalid job allocation: machine %d ('%s') is currently computing jobs (these ones:"
                       " {%s}) whereas space sharing is forbidden. Space sharing can be enabled via an option,"
                       " try --help to display the available options", machine->id, machine->name.c_str(),
                       machine->jobs_being_computed_as_string().c_str());
        }
    }

    if (data->context->energy_used)
    {
        // Check that every machine is in a computation pstate
        for (auto machine_id_it = allocation->machine_ids.elements_begin(); machine_id_it != allocation->machine_ids.elements_end(); ++machine_id_it)
        {
            int machine_id = *machine_id_it;
            Machine * machine = data->context->machines[machine_id];
            int ps = MSG_host_get_pstate(machine->host);
            (void) ps; // Avoids a warning if assertions are ignored
            xbt_assert(machine->has_pstate(ps));
            xbt_assert(machine->pstates[ps] == PStateType::COMPUTATION_PSTATE,
                       "Invalid job allocation: machine %d ('%s') is not in a computation pstate (ps=%d)",
                       machine->id, machine->name.c_str(), ps);
            xbt_assert(machine->state == MachineState::COMPUTING || machine->state == MachineState::IDLE,
                       "Invalid job allocation: machine %d ('%s') cannot compute jobs now (the machine is"
                       " neither computing nor being idle)", machine->id, machine->name.c_str());
        }
    }

    xbt_assert((int)allocation->mapping.size() == job->required_nb_res,
               "Invalid job %s allocation. The job requires %d machines but only %d were given (%s). "
               "Using a different number of machines is only allowed if a custom mapping is specified. "
               "This mapping must specify which allocated machine each executor should use.",
               job->id.to_string().c_str(), job->required_nb_res, (int)allocation->mapping.size(),
               allocation->machine_ids.to_string_hyphen().c_str());

    // Let's generate the hosts used by the job
    allocation->hosts.clear();
    allocation->hosts.reserve(job->required_nb_res);

    for (unsigned int executor_id = 0; executor_id < allocation->mapping.size(); ++executor_id)
    {
        int machine_id_within_allocated_resources = allocation->mapping[executor_id];
        int machine_id = allocation->machine_ids[machine_id_within_allocated_resources];

        allocation->hosts.push_back(data->context->machines[machine_id]->host);
    }

    xbt_assert((int)allocation->hosts.size() == job->required_nb_res,
               "Invalid number of hosts (expected %d, got %d)",
               job->required_nb_res, (int)allocation->hosts.size());

    ExecuteJobProcessArguments * exec_args = new ExecuteJobProcessArguments;
    exec_args->context = data->context;
    exec_args->allocation = allocation;
    exec_args->notify_server_at_end = true;
    string pname = "job_" + job->id.to_string();
    msg_process_t process = MSG_process_create(pname.c_str(), execute_job_process,
                                               (void*)exec_args,
                                               data->context->machines[allocation->machine_ids.first_element()]->host);
    job->execution_processes.insert(process);
}

void check_submitted_and_completed(ServerData * data)
{
    if (!data->all_jobs_submitted_and_completed && // guard to prevent multiple SIMULATION_ENDS events
        data->nb_submitters_finished == data->nb_submitters && // all submitters must have finished
        data->nb_completed_jobs == data->nb_submitted_jobs && // all submitted jobs must have finished
        // If dynamic submission is enabled, it must be finished
        (!data->context->submission_sched_enabled || data->context->submission_sched_finished))
    {
        XBT_INFO("It seems that all jobs have been submitted and completed!");
        data->all_jobs_submitted_and_completed = true;

        data->context->proto_writer->append_simulation_ends(MSG_get_clock());
    }
}

/**
 * @file server.cpp
 * @brief Contains functions related to the general orchestration of the simulation
 */

#include "server.hpp"

#include <chrono>
#include <string>
#include <set>
#include <stdexcept>
#include <memory>

#include <boost/algorithm/string.hpp>

#include <simgrid/s4u.hpp>

#include "context.hpp"
#include "ipp.hpp"
#include "jobs_execution.hpp"
#include "periodic.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(server, "server"); //!< Logging

using namespace std;

void server_process(BatsimContext * context)
{
    ServerData * data = new ServerData;
    data->context = context;
    data->sched_ready = true;

    // Say hello to the external decision process.
    context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
    context->proto_msg_builder->add_batsim_hello("TODO");
    finish_message_and_call_edc(data);

    // Prepare a handler map to react to events
    std::map<IPMessageType, std::function<void(ServerData *, IPMessage *)>> handler_map;
    handler_map[IPMessageType::JOB_SUBMITTED] = server_on_job_submitted;
    handler_map[IPMessageType::SCHED_JOB_REGISTERED] = server_on_register_job;
    handler_map[IPMessageType::SCHED_PROFILE_REGISTERED] = server_on_register_profile;
    handler_map[IPMessageType::JOB_COMPLETED] = server_on_job_completed;
    handler_map[IPMessageType::SCHED_CHANGE_HOST_PSTATE] = server_on_pstate_modification;
    handler_map[IPMessageType::SCHED_EXECUTE_JOB] = server_on_execute_job;
    handler_map[IPMessageType::SCHED_HELLO] = server_on_edc_hello;
    handler_map[IPMessageType::SCHED_REJECT_JOB] = server_on_reject_job;
    handler_map[IPMessageType::SCHED_KILL_JOBS] = server_on_kill_jobs;
    handler_map[IPMessageType::SCHED_CREATE_PROBE] = server_on_create_probe;
    handler_map[IPMessageType::SCHED_STOP_PROBE] = server_on_stop_probe;
    handler_map[IPMessageType::SCHED_CALL_ME_LATER] = server_on_call_me_later;
    handler_map[IPMessageType::SCHED_STOP_CALL_ME_LATER] = server_on_stop_call_me_later;
    handler_map[IPMessageType::SCHED_READY] = server_on_sched_ready;
    handler_map[IPMessageType::ONESHOT_REQUESTED_CALL] = server_on_oneshot_requested_call;
    handler_map[IPMessageType::PERIODIC_TRIGGER] = server_on_periodic_trigger;
    handler_map[IPMessageType::PERIODIC_ENTITY_STOPPED] = server_on_periodic_entity_stopped;
    handler_map[IPMessageType::KILLING_DONE] = server_on_killing_done;
    handler_map[IPMessageType::SUBMITTER_HELLO] = server_on_submitter_hello;
    handler_map[IPMessageType::SUBMITTER_BYE] = server_on_submitter_bye;
    handler_map[IPMessageType::SWITCHED_ON] = server_on_switched;
    handler_map[IPMessageType::SWITCHED_OFF] = server_on_switched;
    handler_map[IPMessageType::SCHED_END_DYNAMIC_REGISTRATION] = server_on_end_dynamic_registration;
    handler_map[IPMessageType::SCHED_FORCE_SIMULATION_STOP] = server_on_force_simulation_stop;
    handler_map[IPMessageType::EXTERNAL_EVENTS_OCCURRED] = server_on_external_events_occurred;

    /* Currently, there is one job submtiter per input file (workload or workflow).
       As workflows use an inner workload, calling nb_static_workloads() should
       be enough. The dynamic job submitter (from the decision process) is not part
       of this count as it can finish then restart... */
    ServerData::SubmitterCounters job_counters;
    job_counters.expected_nb_submitters = context->workloads.nb_static_workloads();
    data->submitter_counters[SubmitterType::JOB_SUBMITTER] = job_counters;

    ServerData::SubmitterCounters external_event_counters;
    external_event_counters.expected_nb_submitters = static_cast<unsigned int>(context->external_event_lists.size());
    data->submitter_counters[SubmitterType::EXTERNAL_EVENT_SUBMITTER] = external_event_counters;

    data->jobs_to_be_deleted.clear();

    // Start an actor dedicated to trigger periodic events (from requested calls and probes)
    auto periodic_actor = simgrid::s4u::Actor::create("periodic", simgrid::s4u::this_actor::get_host(), periodic_main_actor, context);

    // Simulation loop
    while (!data->end_of_simulation_ack_received)
    {
        // Wait and receive a message from a node or the request-reply process...
        IPMessage * message = receive_message("server");
        XBT_DEBUG("Server received a message of type %s.",
                 ip_message_type_to_string(message->type).c_str());

        // Handle the message
        xbt_assert(handler_map.count(message->type) == 1,
                   "The server does not know how to handle message type %s.",
                   ip_message_type_to_string(message->type).c_str());
        auto handler_function = handler_map[message->type];
        handler_function(data, message);

        // Delete the message
        delete message;

        // Let's send a message to the scheduler if needed
        if (data->sched_ready &&                     // The scheduler must be ready
                 !data->end_of_simulation_ack_received && // The simulation must NOT be finished
                 mailbox_empty("server")                  // The server mailbox must be empty
                )
        {
            if (data->simulation_stop_asked)
            {
                // To trigger the SIMULATION_ENDS event
                data->context->proto_msg_builder->clear(simgrid::s4u::Engine::get_clock());
            }

            if (context->proto_msg_builder->has_events()) // There is something to send to the scheduler
            {
                finish_message_and_call_edc(data);
                if (!data->jobs_to_be_deleted.empty())
                {
                    data->context->workloads.delete_jobs(data->jobs_to_be_deleted,
                                                         context->garbage_collect_profiles);
                    data->jobs_to_be_deleted.clear();
                }
            }
            else // There is no event to send to the scheduler
            {
                // Check if the simulation is finished
                if (is_simulation_finished(data) &&
                    !data->end_of_simulation_sent)
                {
                    XBT_INFO("The simulation seems finished.");
                    send_message("periodic", IPMessageType::DIE, nullptr);

                    data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
                    data->context->proto_msg_builder->add_simulation_ends();
                    finish_message_and_call_edc(data);
                    data->end_of_simulation_sent = true;
                }
            }
        }

    } // end of while

    XBT_INFO("Simulation is finished!");

    // Is simulation also finished for the decision process?
    xbt_assert(data->end_of_simulation_sent, "Left simulation loop, but the SIMULATION_ENDS message has not been sent to the scheduler.");
    xbt_assert(data->end_of_simulation_ack_received, "Left simulation loop, but the decision process did not ACK the SIMULATION_ENDS message.");

    // Bypass checks for normal end of simulation
    if (!data->simulation_stop_asked)
    {
        // Are there still pending actions in Batsim?
        xbt_assert(data->sched_ready, "Left simulation loop, but a call to the decision process is ongoing.");
        xbt_assert(data->nb_running_jobs == 0, "Left simulation loop, but some jobs are running.");
        xbt_assert(data->nb_switching_machines == 0, "Left simulation loop, but some machines are being switched.");
        xbt_assert(data->killer_actors.empty(), "Left simulation loop, but some killer processes (used to kill jobs) are running.");
        xbt_assert(data->nb_callmelater_entities == 0, "Left simulation loop, but some entities used to manage CALL_ME_LATER messages are running.");
        xbt_assert(data->nb_probe_entities == 0, "Left simulation loop, but some entities used to manage probes are running.");

        // Consistency
        xbt_assert(data->nb_completed_jobs == data->nb_submitted_jobs, "All submitted jobs have not been completed (either executed and finished, or rejected).");
    }

    delete data;
}

void finish_message_and_call_edc(ServerData * data)
{
    auto context = data->context;

    // finalize the message and serialize it
    context->proto_msg_builder->finish_message(simgrid::s4u::Engine::get_clock());

    uint8_t * what_happened_buffer = nullptr;
    uint32_t what_happened_buffer_size = 0u;
    batprotocol::serialize_message(*context->proto_msg_builder, context->edc_json_format, (const uint8_t**)&what_happened_buffer, &what_happened_buffer_size);

    // call the external decision component
    uint8_t * decisions_buffer = nullptr;
    uint32_t decisions_buffer_size = 0u;
    try
    {
        if (context->edc_json_format)
        {
            XBT_INFO("Sending '%s'", (const char*) what_happened_buffer);
        }

        auto start = chrono::steady_clock::now();

        context->edc->take_decisions(what_happened_buffer, what_happened_buffer_size, &decisions_buffer, &decisions_buffer_size);

        auto end = chrono::steady_clock::now();
        long double elapsed_microseconds = static_cast<long double>(chrono::duration <long double, micro> (end - start).count());
        context->microseconds_used_by_scheduler += elapsed_microseconds;

        if (context->edc_json_format)
        {
            XBT_INFO("Received '%s'", (char *)decisions_buffer);
        }
    }
    catch(const std::runtime_error & error)
    {
        XBT_INFO("Runtime error received: %s", error.what());
        XBT_INFO("Flushing output files...");

        free(data);
        data = nullptr;

        finalize_batsim_outputs(context);

        XBT_INFO("Output files flushed. Aborting execution now.");
        throw runtime_error("Execution aborted (communication with external decision component failed)");
    }

    // parse the decisions and store them in an inter-actor message list
    double now = -1;
    std::shared_ptr<std::vector<IPMessageWithTimestamp> > messages(new std::vector<IPMessageWithTimestamp>());
    protocol::parse_batprotocol_message(decisions_buffer, decisions_buffer_size, now, messages, context);

    // the what_happened buffer is no longer needed, the associated MessageBuilder can be cleared
    context->proto_msg_builder->clear(simgrid::s4u::Engine::get_clock());

    // inject decisions from another actor, so the server can receive them
    data->sched_ready = false;
    data->sched_req_rep_actor = simgrid::s4u::Actor::create("Scheduler REQ-REP", simgrid::s4u::this_actor::get_host(),
        edc_decisions_injector, messages, now
    );
}

void server_on_submitter_hello(ServerData * data,
                               IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    xbt_assert(!data->end_of_simulation_sent,
               "A new submitter said hello but the simulation is finished... Aborting.");
    auto * message = static_cast<SubmitterHelloMessage *>(task_data->data);

    xbt_assert(data->submitters.count(message->submitter_name) == 0,
               "Invalid new submitter '%s': a submitter with the same name already exists!",
               message->submitter_name.c_str());

    ServerData::Submitter * submitter = new ServerData::Submitter;
    submitter->mailbox = message->submitter_name;
    submitter->should_be_called_back = message->enable_callback_on_job_completion;

    data->submitters[message->submitter_name] = submitter;

    SubmitterType submitter_type = message->submitter_type;
    ++data->submitter_counters[submitter_type].nb_submitters;

    XBT_DEBUG("New %s submitter said hello. Number of polite %s submitters: %d",
              submitter_type_to_string(submitter_type).c_str(),
              submitter_type_to_string(submitter_type).c_str(),
              data->submitter_counters[submitter_type].nb_submitters);

}

void server_on_submitter_bye(ServerData * data,
                             IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<SubmitterByeMessage *>(task_data->data);

    xbt_assert(data->submitters.count(message->submitter_name) == 1, "inconsistency: expected 1 submitter with name '%s', got %lu", message->submitter_name.c_str(), data->submitters.count(message->submitter_name));
    delete data->submitters[message->submitter_name];
    data->submitters.erase(message->submitter_name);

    SubmitterType submitter_type = message->submitter_type;

    ++data->submitter_counters[submitter_type].nb_submitters_finished;

    XBT_DEBUG("%s submitter said goodbye. Number of finished %s submitters: %d",
              submitter_type_to_string(submitter_type).c_str(),
              submitter_type_to_string(submitter_type).c_str(),
              data->submitter_counters[submitter_type].nb_submitters);

    if (submitter_type == SubmitterType::EXTERNAL_EVENT_SUBMITTER)
    {
        data->context->external_event_submitter_actors.erase(message->submitter_name);

        if(data->submitter_counters[submitter_type].nb_submitters_finished == data->submitter_counters[submitter_type].expected_nb_submitters)
        {
            data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
            data->context->proto_msg_builder->add_all_static_external_events_have_been_injected();
        }
    }
    else if (submitter_type == SubmitterType::JOB_SUBMITTER)
    {
        data->context->job_submitter_actors.erase(message->submitter_name);

        if(data->submitter_counters[submitter_type].nb_submitters_finished == data->submitter_counters[submitter_type].expected_nb_submitters)
        {
            data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
            data->context->proto_msg_builder->add_all_static_jobs_have_been_submitted();
        }

        if (message->is_workflow_submitter)
        {
            data->nb_workflow_submitters_finished++;
        }
    }
}

void server_on_job_completed(ServerData * data,
                             IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<JobCompletedMessage *>(task_data->data);

    if (data->origin_of_jobs.count(message->job->id) == 1)
    {
        // Let's call the submitter which submitted the job back
        SubmitterJobCompletionCallbackMessage * msg = new SubmitterJobCompletionCallbackMessage;
        msg->job_id = message->job->id;

        ServerData::Submitter * submitter = data->origin_of_jobs.at(message->job->id);
        dsend_message(submitter->mailbox, IPMessageType::SUBMITTER_CALLBACK, static_cast<void*>(msg));

        data->origin_of_jobs.erase(message->job->id);
    }

    data->nb_running_jobs--;
    xbt_assert(data->nb_running_jobs >= 0, "inconsistency: no jobs are running");
    data->nb_completed_jobs++;
    xbt_assert(data->nb_completed_jobs + data->nb_running_jobs <= data->nb_submitted_jobs, "inconsistency: nb_completed_jobs + nb_running_jobs > nb_submitted_jobs");
    auto job = message->job;

    XBT_INFO("Job %s has COMPLETED with return code %d. %d jobs completed so far",
             job->id.to_cstring(), job->return_code, data->nb_completed_jobs);

    data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
    data->context->proto_msg_builder->add_job_completed(
                job->id.to_string(),
                protocol::job_state_to_final_job_state(job->state),
                job->return_code);

    data->context->jobs_tracer.write_job(job);
    data->jobs_to_be_deleted.push_back(message->job->id);
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

    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<JobSubmittedMessage *>(task_data->data);

    ServerData::Submitter * submitter = data->submitters.at(message->submitter_name);
    for (JobPtr & job : message->jobs)
    {
        if (submitter->should_be_called_back)
        {
            xbt_assert(data->origin_of_jobs.count(job->id) == 0, "inconsistency: submitter should be called back but job's submitter has not been tracked");
            data->origin_of_jobs[job->id] = submitter;
        }

        XBT_DEBUG("Job received: %s", job->id.to_cstring());

        XBT_DEBUG("Workloads: %s", data->context->workloads.to_string().c_str());

        // Update control information
        job->state = JobState::JOB_STATE_SUBMITTED;
        ++data->nb_submitted_jobs;
        XBT_INFO("Job %s SUBMITTED. %d jobs submitted so far", job->id.to_cstring(), data->nb_submitted_jobs);

        data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
        if (data->context->forward_profiles_on_job_submission)
        {
            data->context->proto_msg_builder->add_job_submitted(job->id.to_string(), protocol::to_job(*job), job->submission_time, job->profile->name, protocol::to_profile(*(job->profile)));
        }
        else
        {
            data->context->proto_msg_builder->add_job_submitted(job->id.to_string(), protocol::to_job(*job), job->submission_time);
        }
    }
}


void server_on_external_events_occurred(ServerData * data,
                              IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<ExternalEventsOccurredMessage *>(task_data->data);

    for (const ExternalEvent * event : message->occurred_events)
    {
        data->context->proto_msg_builder->add_external_event_occurred(protocol::to_external_event(*event));
    }
}

void server_on_pstate_modification(ServerData * data,
                                   IPMessage * task_data)
{

    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");

    auto * message = static_cast<ChangeHostPStateMessage *>(task_data->data);

    if (data->context->energy_used)
    {
        data->context->energy_tracer.add_pstate_change(simgrid::s4u::Engine::get_clock(), message->machine_ids,
                                                       message->new_pstate);
    }

    if (data->context->trace_pstate_changes)
    {
        data->context->pstate_tracer.add_pstate_change(simgrid::s4u::Engine::get_clock(), message->machine_ids, message->new_pstate);
    }

    for (auto machine_it = message->machine_ids.elements_begin();
         machine_it != message->machine_ids.elements_end();
         ++machine_it)
    {
        const int machine_id = *machine_it;
        Machine * machine = data->context->machines[machine_id];
        unsigned long curr_pstate = machine->host->get_pstate();

        XBT_INFO("Switching machine %d ('%s') pstate : %lu -> %lu.", machine->id,
                 machine->name.c_str(), curr_pstate, message->new_pstate);
        machine->host->set_pstate(message->new_pstate);
        xbt_assert(machine->host->get_pstate() == message->new_pstate, "pstate inconsistency: the desired pstate has not been set");
    }

    data->context->proto_msg_builder->add_host_pstate_changed(message->machine_ids.to_string_hyphen(), message->new_pstate);

    // TODO: UPDATE the switch ON/OFF mecanism
    /*data->context->current_switches.add_switch(message->machine_ids, message->new_pstate);

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

    // The pstate is set to an invalid one to know the machines are in transition.
    data->context->pstate_tracer.add_pstate_change(simgrid::s4u::Engine::get_clock(), message->machine_ids,
                                                   transition_state);

    // Let's mark that some switches have been requested
    data->context->nb_grouped_switches++;
    data->context->nb_machine_switches += message->machine_ids.size();

    for (auto machine_it = message->machine_ids.elements_begin();
         machine_it != message->machine_ids.elements_end();
         ++machine_it)
    {
        const int machine_id = *machine_it;
        Machine * machine = data->context->machines[machine_id];
        int curr_pstate = machine->host->get_pstate();

        if (machine->pstates[curr_pstate] == PStateType::COMPUTATION_PSTATE)
        {
            if (machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE)
            {
                XBT_INFO("Switching machine %d ('%s') pstate : %d -> %d.", machine->id,
                         machine->name.c_str(), curr_pstate, message->new_pstate);
                machine->host->set_pstate(message->new_pstate);
                xbt_assert(machine->host->get_pstate() == message->new_pstate, "pstate inconsistency: the desired pstate has not been set");

                IntervalSet all_switched_machines;
                if (data->context->current_switches.mark_switch_as_done(machine->id, message->new_pstate,
                                                                        all_switched_machines,
                                                                        data->context))
                {
                    data->context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                               std::to_string(message->new_pstate),
                                                                               simgrid::s4u::Engine::get_clock());
                    // TODO: handle me in batprotocol
                }
            }
            else if (machine->pstates[message->new_pstate] == PStateType::SLEEP_PSTATE)
            {
                machine->update_machine_state(MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING);

                string pname = "switch ON " + to_string(machine_id);
                simgrid::s4u::Actor::create(pname.c_str(), machine->host, switch_off_machine_process,
                                            data->context, machine_id, message->new_pstate);

                ++data->nb_switching_machines;
            }
            else
            {
                XBT_ERROR("Switching from a computation pstate to an invalid pstate on machine %d ('%s') : %d -> %d",
                          machine->id, machine->name.c_str(), curr_pstate, message->new_pstate);
            }
        }
        else if (machine->pstates[curr_pstate] == PStateType::SLEEP_PSTATE)
        {
            xbt_assert(machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE,
                    "Switching from a sleep pstate to a non-computation pstate on machine %d ('%s') : %d -> %d, which is forbidden",
                    machine->id, machine->name.c_str(), curr_pstate, message->new_pstate);

            machine->update_machine_state(MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING);

            string pname = "switch OFF " + to_string(machine_id);
            simgrid::s4u::Actor::create(pname.c_str(), machine->host, switch_on_machine_process,
                                        data->context, machine_id, message->new_pstate);

            ++data->nb_switching_machines;
        }
        else
        {
            XBT_ERROR("Machine %d ('%s') has an invalid pstate : %d", machine->id, machine->name.c_str(), curr_pstate);
        }
    }

    if (data->context->trace_machine_states)
    {
        data->context->machine_state_tracer.write_machine_states(simgrid::s4u::Engine::get_clock());
    }
    */

}

void server_on_oneshot_requested_call(ServerData * data,
                                      IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<OneShotRequestedCallMessage *>(task_data->data);
    auto & msg = message->call;

    --data->nb_callmelater_entities;

    data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
    data->context->proto_msg_builder->add_requested_call(msg.call_id, msg.is_last_periodic_call);
}

void server_on_periodic_trigger(ServerData * data,
                                IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<PeriodicTriggerMessage *>(task_data->data);

    data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());

    for (auto & call : message->calls) {
        data->context->proto_msg_builder->add_requested_call(call.call_id, call.is_last_periodic_call);
        if (call.is_last_periodic_call)
            --data->nb_callmelater_entities;
    }

    for (auto * probe_data : message->probes_data) {
        if (probe_data->is_last_periodic)
            --data->nb_probe_entities;

        std::shared_ptr<batprotocol::ProbeData> pdata;
        switch (probe_data->data_type) {
            case batprotocol::fb::ProbeData_VectorialProbeData: {
                pdata = batprotocol::ProbeData::make_vectorial(std::make_shared<std::vector<double>>(std::move(probe_data->vectorial_data)));
            } break;
            case batprotocol::fb::ProbeData_AggregatedProbeData: {
                pdata = batprotocol::ProbeData::make_aggregated(probe_data->aggregated_data);
            } break;
            default: {
                xbt_assert(false, "unimplemented probe data type");
            } break;
        }

        switch(probe_data->resource_type) {
            case batprotocol::fb::Resources_HostResources: {
                pdata->set_resources_as_hosts(probe_data->hosts.to_string_hyphen());
            } break;
            case batprotocol::fb::Resources_LinkResources: {
                pdata->set_resources_as_links(std::make_shared<std::vector<std::string> >(std::move(probe_data->links)));
            } break;
            default: {
                xbt_assert(false, "unimplemented probe resource type");
            } break;
        }

        data->context->proto_msg_builder->add_probe_data_emitted(
            probe_data->probe_id, probe_data->metrics, pdata,
            probe_data->manually_triggered, probe_data->nb_emitted, probe_data->nb_triggered
        );
    }
}

void server_on_periodic_entity_stopped(ServerData * data,
                                       IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<PeriodicEntityStoppedMessage *>(task_data->data);

    if (message->is_probe)
        --data->nb_probe_entities;
    else if (message->is_call_me_later)
        --data->nb_callmelater_entities;
}

void server_on_sched_ready(ServerData * data,
                           IPMessage * task_data)
{
    (void) task_data;
    data->sched_ready = true;

    if (data->end_of_simulation_sent)
    {
        data->end_of_simulation_ack_received = true;
    }
}

void server_on_switched(ServerData * data,
                        IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");

    xbt_assert(false, "Switching ON/OFF machines handling is not implemented yet");
    // TODO: handle me with batprotocol

    /*auto * message = static_cast<SwitchMessage *>(task_data->data);
    xbt_assert(data->context->machines.exists(message->machine_id), "machine %d does not exist", message->machine_id);
    Machine * machine = data->context->machines[message->machine_id];
    (void) machine; // Avoids a warning if assertions are ignored
    xbt_assert(machine->host->get_pstate() == message->new_pstate, "pstate inconsistency: the desired pstate has not been set");

    IntervalSet all_switched_machines;
    if (data->context->current_switches.mark_switch_as_done(message->machine_id, message->new_pstate,
                                                            all_switched_machines, data->context))
    {
        if (data->context->trace_machine_states)
        {
            data->context->machine_state_tracer.write_machine_states(simgrid::s4u::Engine::get_clock());
        }

        data->context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                   std::to_string(message->new_pstate),
                                                                   simgrid::s4u::Engine::get_clock());
        // TODO: implement me in batprotocol
    }

    --data->nb_switching_machines;*/
}

void server_on_killing_done(ServerData * data,
                            IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<KillingDoneMessage *>(task_data->data);

    vector<string> really_killed_job_ids_str;
    vector<string> job_ids_str;
    job_ids_str.reserve(message->kill_jobs_message->job_ids.size());

    for (auto & job_id : message->kill_jobs_message->job_ids)
    {
        job_ids_str.push_back(job_id.to_string());

        const auto job = data->context->workloads.job_at(job_id);
        if (job->state == JobState::JOB_STATE_COMPLETED_KILLED)
        {
            data->nb_running_jobs--;
            xbt_assert(data->nb_running_jobs >= 0, "inconsistency: no jobs are running");
            data->nb_completed_jobs++;
            xbt_assert(data->nb_completed_jobs + data->nb_running_jobs <= data->nb_submitted_jobs, "inconsistency: nb_completed_jobs + nb_running_jobs > nb_submitted_jobs");

            really_killed_job_ids_str.push_back(job_id.to_string());

            // Also add a job completed message for the jobs that have really been killed
            data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());
            data->context->proto_msg_builder->add_job_completed(
                job->id.to_string(),
                protocol::job_state_to_final_job_state(job->state),
                job->return_code
            );

            data->context->jobs_tracer.write_job(job);
            data->jobs_to_be_deleted.push_back(job->id);
        }
    }

    if (message->acknowledge_kill_on_protocol)
    {
        XBT_INFO("Finished the requested kills of jobs {%s}. The following jobs have REALLY been killed: {%s})",
                 boost::algorithm::join(job_ids_str, ",").c_str(),
                 boost::algorithm::join(really_killed_job_ids_str, ",").c_str());

        data->context->proto_msg_builder->add_jobs_killed(job_ids_str, message->jobs_progress, message->profiles);
    }

    data->killer_actors.erase(message->kill_jobs_message);
}

void server_on_end_dynamic_registration(ServerData * data,
                                        IPMessage * task_data)
{
    (void) task_data;

    data->context->registration_sched_finished = true;
}

void server_on_force_simulation_stop(ServerData * data,
                                     IPMessage * task_data)
{
    (void) task_data;

    XBT_DEBUG("Handling force simulation stop");
    data->simulation_stop_asked = true;

    // Everything for the simulation to forcefully stop without deadlock

    // Kill all SimGrid actors

    // Static workloads and workflows submitters
    for (auto it : data->context->job_submitter_actors)
    {
        it.second->kill();
    }
    data->context->job_submitter_actors.clear();

    // Static external events submitters
    for (auto it : data->context->external_event_submitter_actors)
    {
        it.second->kill();
    }
    data->context->external_event_submitter_actors.clear();

    // Killer_processes
    for (auto it : data->killer_actors)
    {
        it.second->kill();
    }
    data->killer_actors.clear();

    // Running job actors
    for (Machine * machine : data->context->machines.machines())
    {
        for (JobPtr job : machine->jobs_being_computed)
        {
            for (simgrid::s4u::ActorPtr actor : job->execution_actors)
            {
                actor->kill();
            }
            job->execution_actors.clear();
        }
    }

    // TODO: once switching ON/OFF of machines is implemented need to kill the created actors here as well

    // Kill the scheduler REQ-REP actor
    data->sched_req_rep_actor->kill();

    // Clear the server mailbox (no need to handle next scheduling events)
    clear_mailbox("server");
    data->sched_ready = true;

    // Clear the events to send to the EDC
    data->context->proto_msg_builder->clear(simgrid::s4u::Engine::get_clock());
}

void server_on_register_job(ServerData * data,
                            IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<JobRegisteredByEDCMessage *>(task_data->data);
    JobPtr job = message->job;
    JobIdentifier job_id = job->id;

    // Retrieve the workload of the job (or create it)
    Workload * workload;
    if (data->context->workloads.exists(job_id.workload_name()))
    {
        xbt_assert(!data->context->workloads.job_is_registered(job_id),
                   "Invalid new job registration: '%s' already exists in the workload.", job_id.to_cstring());
        workload = data->context->workloads.at(job_id.workload_name());
    }
    else
    {
        workload = Workload::new_dynamic_workload(job_id.workload_name());
        data->context->workloads.insert_workload(job_id.workload_name(), workload);
        XBT_INFO("Created new dynamic workload %s", job_id.workload_name().c_str());
    }

    // Retrieve the Profile
    ProfilePtr profile;

    // Expecting a "workload!name" syntax in profile_id, but "name" is also allowed (in this case, only profiles from the job's workload can be used)
    auto tsplit = message->profile_id.find('!');
    if (tsplit == std::string::npos)
    {
        // Profile_id does not contain a workload name, add the job's workload to it
        std::string profile_name = workload->name + "!" + message->profile_id;

        // check if profile is in the Job's workload
        xbt_assert(workload->profiles->exists(profile_name),
                    "Invalid new job registration for '%s': the associated profile '%s' does not exist",
                    job_id.to_cstring(), profile_name.c_str());

        profile = workload->profiles->at(profile_name);
    }
    else
    {
        // Profile_id contains the workload name
        std::string profile_workload_str = message->profile_id.substr(0, tsplit);
        xbt_assert(data->context->workloads.exists(profile_workload_str),
                   "Invalid new job resgistration for '%s': the profile's workload does not exist (%s)",
                   job_id.to_cstring(), profile_workload_str.c_str());

        xbt_assert(data->context->workloads.profile_is_registered(message->profile_id, profile_workload_str),
                   "Invalid new job registration for '%s': the profile does not exist (%s).",
                   job_id.to_cstring(), message->profile_id.c_str());

        profile = data->context->workloads.at(profile_workload_str)->profiles->at(message->profile_id);
    }

    // Let's update some global and job parameters
    ++data->nb_submitted_jobs;

    job->profile = profile;
    job->workload = workload;
    job->state = JobState::JOB_STATE_SUBMITTED;
    job->submission_time = simgrid::s4u::Engine::get_clock();

    workload->check_single_job_validity(message->job);
    workload->jobs->add_job(message->job);

    XBT_INFO("Adding dynamically registered job '%s' to workload '%s'",
             job_id.job_name().c_str(), job_id.workload_name().c_str());

    if (data->context->registration_sched_ack)
    {
        // TODO: handle the multi-EDC

        data->context->proto_msg_builder->set_current_time(simgrid::s4u::Engine::get_clock());

        if (data->context->forward_profiles_on_job_submission)
        {
            data->context->proto_msg_builder->add_job_submitted(job->id.to_string(), protocol::to_job(*job), job->submission_time, job->profile->name, protocol::to_profile(*(job->profile)));
        }
        else
        {
            data->context->proto_msg_builder->add_job_submitted(job->id.to_string(), protocol::to_job(*job), job->submission_time);
        }
    }
}

void server_on_register_profile(ServerData * data,
                                IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<ProfileRegisteredByEDCMessage *>(task_data->data);

    // Retrieve the workload and profile names
    auto tsplit = message->profile_id.find('!');
    std::string workload_name = message->profile_id.substr(0, tsplit);
    std::string profile_name = message->profile_id;

    // Retrieve the workload or create it
    Workload * workload;
    if (data->context->workloads.exists(workload_name))
    {
        xbt_assert(!data->context->workloads.profile_is_registered(profile_name, workload_name),
                   "Invalid new profile registration: '%s' already exists in the workload.", message->profile_id.c_str());
        workload = data->context->workloads.at(workload_name);
    }
    else
    {
        workload = Workload::new_dynamic_workload(workload_name);
        data->context->workloads.insert_workload(workload_name, workload);
        XBT_INFO("Created new dynamic workload %s", workload_name.c_str());
    }

    message->profile->name = profile_name;
    message->profile->workload = workload;

    // check profile validity. For composition profiles, check that the subprofiles already exist and fill the appropriate vector of Profiles
    workload->check_single_profile_validity(message->profile);

    // Add the profile in the workload
    workload->profiles->add_profile(profile_name, message->profile);
    XBT_INFO("Adding dynamically registered profile '%s' to workload '%s'.",
             profile_name.c_str(), workload_name.c_str());

}

void server_on_reject_job(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<RejectJobMessage *>(task_data->data);
    auto job = data->context->workloads.job_at(message->job_id);

    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
        "Invalid job rejection received: job '%s' has state='%s' which is not SUBMITTED.",
        job->id.to_cstring(), job_state_to_string(job->state).c_str()
    );

    job->state = JobState::JOB_STATE_REJECTED;
    data->nb_completed_jobs++;

    XBT_INFO("Job '%s' has been rejected", job->id.to_cstring());

    data->context->jobs_tracer.write_job(job);
    data->jobs_to_be_deleted.push_back(message->job_id);
}

void server_on_kill_jobs(ServerData * data, IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<KillJobsMessage *>(task_data->data);

    // Traverse jobs and discard those whose kill has already been requested (to avoid double kill of jobs).
    for (auto job_id_it = message->job_ids.begin(); job_id_it != message->job_ids.end() ; )
    {
        JobPtr job = data->context->workloads.job_at(*job_id_it);

        if (!job->kill_requested)
        {
            // Check the job state
            xbt_assert(job->state == JobState::JOB_STATE_RUNNING || job->is_complete(),
                "Invalid KILL_JOB: job_id '%s' is neither being executed nor completed, it has state='%s'",
                job->id.to_cstring(), job_state_to_string(job->state).c_str()
            );

            // Mark that the job kill has been requested
            job->kill_requested = true;

            ++job_id_it;
        }
        else
        {
            // Erase the current job from the vector and continue iterating over it.
            job_id_it = message->job_ids.erase(job_id_it);
        }
    }


    simgrid::s4u::ActorPtr kill_actor = simgrid::s4u::Actor::create("killer_process", simgrid::s4u::this_actor::get_host(), killer_process,
        data->context, message, JobState::JOB_STATE_COMPLETED_KILLED, true
    );
    // Store the killer actor
    data->killer_actors[message] = kill_actor;
}

void server_on_create_probe(ServerData * data,
                            IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<CreateProbeMessage *>(task_data->data);

    ++data->nb_probe_entities;

    xbt_assert(message->is_periodic, "non-periodic probes are not implemented");
    send_message("periodic", IPMessageType::SCHED_CREATE_PROBE, task_data->data);
}

void server_on_stop_probe(ServerData * data,
                          IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    send_message("periodic", IPMessageType::SCHED_STOP_PROBE, task_data->data);
}

void server_on_call_me_later(ServerData * data,
                             IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<CallMeLaterMessage *>(task_data->data);

    ++data->nb_callmelater_entities;

    if (message->is_periodic)
    {
        send_message("periodic", IPMessageType::SCHED_CALL_ME_LATER, task_data->data);
    }
    else
    {
        string pname = "oneshot_call_me_later " + message->call_id + " " + to_string(message->target_time);
        double target_time = message->target_time;
        if (message->time_unit == batprotocol::fb::TimeUnit_Millisecond)
            target_time /= 1e3;
        simgrid::s4u::Actor::create(pname.c_str(),
                                    data->context->machines.master_machine()->host,
                                    oneshot_call_me_later_actor, message->call_id, target_time, data);

        // Deallocate memory now for the non-periodic case
        delete message;
        task_data->data = nullptr;
    }
}

void server_on_stop_call_me_later(ServerData * data,
                                  IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    send_message("periodic", IPMessageType::SCHED_STOP_CALL_ME_LATER, task_data->data);
}

void server_on_execute_job(ServerData * data,
                           IPMessage * task_data)
{
    xbt_assert(task_data->data != nullptr,  "inconsistency: task_data has null data");

    // Bind the message memory lifecycle to the corresponding job's lifecycle.
    std::shared_ptr<ExecuteJobMessage> message(static_cast<ExecuteJobMessage *>(task_data->data));
    task_data->data = nullptr;
    auto allocation = message->job_allocation;
    auto job = data->context->workloads.job_at(message->job_id);
    job->execution_request = message;

    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
               "Cannot execute job '%s': its state (%s) is not JOB_STATE_SUBMITTED.",
               job->id.to_cstring(), job_state_to_string(job->state).c_str());

    job->state = JobState::JOB_STATE_RUNNING;

    data->nb_running_jobs++;
    xbt_assert(data->nb_running_jobs <= data->nb_submitted_jobs, "inconsistency: nb_running_jobs > nb_submitted_jobs");

    // Check that every machine can compute the job
    for (auto machine_id_it = allocation->hosts.elements_begin(); machine_id_it != allocation->hosts.elements_end(); ++machine_id_it)
    {
        int machine_id = *machine_id_it;
        Machine * machine = data->context->machines[machine_id];

        xbt_assert(machine->state == MachineState::COMPUTING || machine->state == MachineState::IDLE,
                   "Job '%s': Invalid job allocation ('%s'): machine %d (hostname='%s') cannot compute jobs now "
                   "(the machine is not computing nor idle, its state is '%s')",
                   job->id.to_cstring(),
                   allocation->hosts.to_string_hyphen().c_str(),
                   machine->id, machine->name.c_str(),
                   machine_state_to_string(machine->state).c_str());

        if (data->context->energy_used)
        {
        // Check that every machine is in a computation pstate
        int ps = machine->host->get_pstate();
        (void) ps; // Avoids a warning if assertions are ignored
        xbt_assert(machine->has_pstate(ps), "machine %d has no pstate %d", machine_id, ps);
        xbt_assert(machine->pstates[ps] == PStateType::COMPUTATION_PSTATE,
                   "Job '%s': Invalid job allocation ('%s'): machine %d (hostname='%s') is not in a computation pstate (ps=%d)",
                   job->id.to_cstring(),
                   allocation->hosts.to_string_hyphen().c_str(),
                   machine->id, machine->name.c_str(), ps);
        }
    }

    string pname = "job_" + job->id.to_string();
    auto actor = simgrid::s4u::Actor::create(pname.c_str(),
        data->context->machines[allocation->hosts.first_element()]->host,
        execute_job_process, data->context, job, true
    );
    job->execution_actors.insert(actor);
}

bool is_simulation_finished(ServerData * data)
{
    // Big bypass that won't work anymore with multiple EDC
    if (data->simulation_stop_asked)
    {
        return true;
    }

    return (data->submitter_counters[SubmitterType::JOB_SUBMITTER].nb_submitters_finished == data->submitter_counters[SubmitterType::JOB_SUBMITTER].expected_nb_submitters) &&
           (data->submitter_counters[SubmitterType::EXTERNAL_EVENT_SUBMITTER].nb_submitters_finished == data->submitter_counters[SubmitterType::EXTERNAL_EVENT_SUBMITTER].expected_nb_submitters) &&
           (!data->context->registration_sched_enabled || data->context->registration_sched_finished) && // Dynamic submissions are disabled or finished
           (data->nb_completed_jobs == data->nb_submitted_jobs) && // All submitted jobs have been completed (either computed and finished or rejected)
           (data->nb_running_jobs == 0) && // No jobs are being executed
           (data->nb_switching_machines == 0) && // No machine is being switched
           (data->nb_callmelater_entities == 0) && // No entities related to CALL_ME_LATER are running
           (data->nb_probe_entities == 0) && // No entities related to probes are running
           (data->killer_actors.empty()); // No jobs are being killed
}

void server_on_edc_hello(ServerData *data, IPMessage *task_data)
{
    xbt_assert(!data->sched_said_hello, "External decision component said hello twice!");
    data->sched_said_hello = true;

    (void) task_data;
    xbt_assert(task_data->data != nullptr, "inconsistency: task_data has null data");
    auto * message = static_cast<EDCHelloMessage *>(task_data->data);
    // TODO: check batprotocol version compatibility, store&log scheduler tracability info...

    // TODO: set tunable behavior per EDC, not for all of them
    data->context->registration_sched_enabled = message->requested_simulation_features.dynamic_registration;
    data->context->registration_sched_ack = message->requested_simulation_features.acknowledge_dynamic_jobs;
    data->context->garbage_collect_profiles = !(message->requested_simulation_features.dynamic_registration && message->requested_simulation_features.profile_reuse);
    data->context->forward_profiles_on_simulation_begins = message->requested_simulation_features.forward_profiles_on_simulation_begins;
    data->context->forward_profiles_on_job_submission= message->requested_simulation_features.forward_profiles_on_job_submission;
    data->context->forward_profiles_on_jobs_killed = message->requested_simulation_features.forward_profiles_on_jobs_killed;

    if (data->context->registration_sched_enabled)
    {
        XBT_WARN("Dynamic registration of jobs is ENABLED. The EDC MUST send a FinishRegistrationEvent to let Batsim end the simulation.");
    }
    else
    {
        if (data->context->registration_sched_ack)
        {
            XBT_WARN("Acknowledgement of dynamic jobs is enabled but dynamic registation is diabled.");
        }
    }

    auto simulation_begins = protocol::to_simulation_begins(data->context);
    data->context->proto_msg_builder->add_simulation_begins(simulation_begins);
}

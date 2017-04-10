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

    int nb_completed_jobs = 0;
    int nb_submitted_jobs = 0;
    int nb_scheduled_jobs = 0;
    int nb_submitters = 0;
    int nb_submitters_finished = 0;
    int nb_workflow_submitters_finished = 0;
    int nb_running_jobs = 0;
    int nb_switching_machines = 0;
    int nb_waiters = 0;
    int nb_killers = 0;
    bool sched_ready = true;
    bool all_jobs_submitted_and_completed = false;
    bool end_of_simulation_sent = false;

    // Let's store some information about the submitters
    struct Submitter
    {
        string mailbox;
        bool should_be_called_back;
    };

    map<string, Submitter*> submitters;

    // Let's store the origin of some jobs
    map<JobIdentifier, Submitter*> origin_of_jobs;

    // Let's store the origin of wait queries
    map<std::pair<int,double>, Submitter*> origin_of_wait_queries;

    // Let's tell the Decision process that the simulation is about to begin (and that some data can be read from the data storage)
    context->proto_writer->append_simulation_begins(context->machines.nb_machines(), MSG_get_clock());

    RequestReplyProcessArguments * req_rep_args = new RequestReplyProcessArguments;
    req_rep_args->context = context;
    req_rep_args->send_buffer = context->proto_writer->generate_current_message(MSG_get_clock());
    context->proto_writer->clear();

    MSG_process_create("Scheduler REQ-REP", request_reply_scheduler_process, (void*)req_rep_args, MSG_host_self());
    sched_ready = false;


    // Simulation loop
    while ((nb_submitters == 0) || (nb_submitters_finished < nb_submitters) ||
          (nb_completed_jobs < nb_submitted_jobs) || (!sched_ready) ||
          (nb_switching_machines > 0) || (nb_waiters > 0) || (nb_killers > 0))
    {
        // Let's wait a message from a node or the request-reply process...
        msg_task_t task_received = NULL;
        IPMessage * task_data;
        MSG_task_receive(&(task_received), "server");
        task_data = (IPMessage *) MSG_task_get_data(task_received);

        XBT_INFO( "Server received a message of type %s:", ip_message_type_to_string(task_data->type).c_str());

        switch (task_data->type)
        {
        case IPMessageType::SUBMITTER_HELLO:
        {
            xbt_assert(task_data->data != nullptr);
            xbt_assert(!all_jobs_submitted_and_completed,
                       "A new submitter said hello but all jobs have already been submitted and completed... Aborting.");
            SubmitterHelloMessage * message = (SubmitterHelloMessage *) task_data->data;

            xbt_assert(submitters.count(message->submitter_name) == 0,
                       "Invalid new submitter '%s': a submitter with the same name already exists!",
                       message->submitter_name.c_str());

            nb_submitters++;

            Submitter * submitter = new Submitter;
            submitter->mailbox = message->submitter_name;
            submitter->should_be_called_back = message->enable_callback_on_job_completion;

            submitters[message->submitter_name] = submitter;

            XBT_INFO("New submitter said hello. Number of polite submitters: %d", nb_submitters);

        } break; // end of case SUBMITTER_HELLO

        case IPMessageType::SUBMITTER_BYE:
        {
            xbt_assert(task_data->data != nullptr);
            SubmitterByeMessage * message = (SubmitterByeMessage *) task_data->data;

            xbt_assert(submitters.count(message->submitter_name) == 1);
            submitters.erase(message->submitter_name);

            nb_submitters_finished++;
            if (message->is_workflow_submitter)
                nb_workflow_submitters_finished++;
            XBT_INFO("A submitted said goodbye. Number of finished submitters: %d", nb_submitters_finished);

            if (!all_jobs_submitted_and_completed &&
                nb_completed_jobs == nb_submitted_jobs &&
                nb_submitters_finished == nb_submitters)
            {
                all_jobs_submitted_and_completed = true;
                XBT_INFO("It seems that all jobs have been submitted and completed!");

                context->proto_writer->append_simulation_ends(MSG_get_clock());
            }

        } break; // end of case SUBMITTER_BYE

        case IPMessageType::JOB_COMPLETED:
        {
            xbt_assert(task_data->data != nullptr);
            JobCompletedMessage * message = (JobCompletedMessage *) task_data->data;

            if (origin_of_jobs.count(message->job_id) == 1)
            {
                // Let's call the submitter which submitted the job back
                SubmitterJobCompletionCallbackMessage * msg = new SubmitterJobCompletionCallbackMessage;
                msg->job_id = message->job_id;

                Submitter * submitter = origin_of_jobs.at(message->job_id);
                dsend_message(submitter->mailbox, IPMessageType::SUBMITTER_CALLBACK, (void*) msg);

                origin_of_jobs.erase(message->job_id);
            }

            nb_running_jobs--;
            xbt_assert(nb_running_jobs >= 0);
            nb_completed_jobs++;
            xbt_assert(nb_completed_jobs + nb_running_jobs <= nb_submitted_jobs);
            Job * job = context->workloads.job_at(message->job_id);

            XBT_INFO("Job %s!%d COMPLETED. %d jobs completed so far",
                     job->workload->name.c_str(), job->number, nb_completed_jobs);

            string status = "UNKNOWN";
            if (job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY)
                status = "SUCCESS";
            else if (job->state == JobState::JOB_STATE_COMPLETED_KILLED && job->kill_reason == "Walltime reached")
                status = "TIMEOUT";

            context->proto_writer->append_job_completed(message->job_id.to_string(), status, MSG_get_clock());

            if (!all_jobs_submitted_and_completed &&
                nb_completed_jobs == nb_submitted_jobs &&
                nb_submitters_finished == nb_submitters)
            {
                all_jobs_submitted_and_completed = true;
                XBT_INFO("It seems that all jobs have been submitted and completed!");

                context->proto_writer->append_simulation_ends(MSG_get_clock());
            }
        } break; // end of case JOB_COMPLETED

        case IPMessageType::JOB_SUBMITTED:
        {
            // Ignore all submissions if -k was specified and all workflows have completed
            if ((context->workflows.size() != 0) && (context->terminate_with_last_workflow) &&
                    (nb_workflow_submitters_finished == context->workflows.size()))
            {
                XBT_INFO("Ignoring Job due to -k command-line option");
                break;
            }

            xbt_assert(task_data->data != nullptr);
            JobSubmittedMessage * message = (JobSubmittedMessage *) task_data->data;

            xbt_assert(submitters.count(message->submitter_name) == 1);

            Submitter * submitter = submitters.at(message->submitter_name);
            if (submitter->should_be_called_back)
            {
                xbt_assert(origin_of_jobs.count(message->job_id) == 0);
                origin_of_jobs[message->job_id] = submitter;
            }

            // Let's retrieve the Job from memory (or add it into memory if it is dynamic)
            XBT_INFO("GOT JOB: %s %d\n", message->job_id.workload_name.c_str(), message->job_id.job_number);
            Job * job = context->workloads.add_job_if_not_exists(message->job_id, context);
            job->id = message->job_id.to_string();

            // Update control information
            job->state = JobState::JOB_STATE_SUBMITTED;
            nb_submitted_jobs++;
            XBT_INFO("Job %s SUBMITTED. %d jobs submitted so far",
                     message->job_id.to_string().c_str(), nb_submitted_jobs);

            context->proto_writer->append_job_submitted(job->id, MSG_get_clock());
        } break; // end of case JOB_SUBMITTED

        case IPMessageType::JOB_SUBMITTED_BY_DP:
        {
            xbt_assert(task_data->data != nullptr);
            JobSubmittedByDPMessage * message = (JobSubmittedByDPMessage *) task_data->data;

            xbt_assert(!context->workloads.job_exists(message->job_id),
                       "Bad job submission received from the decision process: job %s already exists.",
                       message->job_id.to_string().c_str());

            // Let's create the workload if it doesn't exist, or retrieve it otherwise
            Workload * workload = nullptr;
            if (context->workloads.exists(message->job_id.workload_name))
                workload = context->workloads.at(message->job_id.workload_name);
            else
            {
                workload = new Workload(message->job_id.workload_name);
                context->workloads.insert_workload(workload->name, workload);
            }

            // Let's parse the user-submitted job
            XBT_INFO("Parsing user-submitted job %s", message->job_id.to_string().c_str());
            Job * job = Job::from_json(message->job_description, workload);
            workload->jobs->add_job(job);

            // Let's parse the profile if needed
            if (!workload->profiles->exists(job->profile))
            {
                XBT_INFO("The profile of user-submitted job '%s' does not exist. Parsing the user-submitted profile '%s'",
                         job->profile.c_str(), message->job_id.to_string().c_str());
                Profile * profile = Profile::from_json(job->profile, message->job_profile);
                workload->profiles->add_profile(job->profile, profile);
            }

        } break; // end of case JOB_SUBMITTED_BY_DP

        case IPMessageType::SCHED_REJECT_JOB:
        {
            xbt_assert(task_data->data != nullptr);
            JobRejectedMessage * message = (JobRejectedMessage *) task_data->data;

            Job * job = context->workloads.job_at(message->job_id);
            job->state = JobState::JOB_STATE_REJECTED;
            nb_completed_jobs++;

            XBT_INFO("Job %d (workload=%s) has been rejected",
                     job->number, job->workload->name.c_str());
        } break; // end of case SCHED_REJECTION

        case IPMessageType::SCHED_KILL_JOB:
        {
            xbt_assert(task_data->data != nullptr);
            KillJobMessage * message = (KillJobMessage *) task_data->data;

            KillerProcessArguments * args = new KillerProcessArguments;
            args->context = context;
            args->jobs_ids = message->jobs_ids;

            for (const JobIdentifier & job_id : args->jobs_ids)
            {
                Job * job = context->workloads.job_at(job_id);
                xbt_assert(job->state == JobState::JOB_STATE_RUNNING ||
                           job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY ||
                           job->state == JobState::JOB_STATE_COMPLETED_KILLED,
                           "Invalid KILL_JOB: job_id '%s' refers to a job not being executed nor completed.",
                           job_id.to_string().c_str());
            }

            MSG_process_create("killer_process", killer_process, (void *) args, MSG_host_self());
            ++nb_killers;
        } break; // end of case SCHED_KILL_JOB

        case IPMessageType::SCHED_CALL_ME_LATER:
        {
            xbt_assert(task_data->data != nullptr);
            CallMeLaterMessage * message = (CallMeLaterMessage *) task_data->data;

            xbt_assert(message->target_time > MSG_get_clock(), "You asked to be awaken in the past! (you ask: %f, it is: %f)", message->target_time, MSG_get_clock());

            WaiterProcessArguments * args = new WaiterProcessArguments;
            args->target_time = message->target_time;

            string pname = "waiter " + to_string(message->target_time);
            MSG_process_create(pname.c_str(), waiter_process, (void*) args, context->machines.master_machine()->host);
            ++nb_waiters;
        } break; // end of case SCHED_CALL_ME_LATER

        case IPMessageType::PSTATE_MODIFICATION:
        {
            xbt_assert(task_data->data != nullptr);
            PStateModificationMessage * message = (PStateModificationMessage *) task_data->data;

            context->current_switches.add_switch(message->machine_ids, message->new_pstate);
            context->energy_tracer.add_pstate_change(MSG_get_clock(), message->machine_ids, message->new_pstate);

            // Let's quickly check whether this is a switchON or a switchOFF
            // Unknown transition states will be set to -42.
            int transition_state = -42;
            Machine * first_machine = context->machines[message->machine_ids.first_element()];
            if (first_machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE)
                transition_state = -1; // means we are switching to a COMPUTATION_PSTATE
            else if (first_machine->pstates[message->new_pstate] == PStateType::SLEEP_PSTATE)
                transition_state = -2; // means we are switching to a SLEEP_PSTATE

            // The pstate is set to an invalid one to know the machines are in transition.
            context->pstate_tracer.add_pstate_change(MSG_get_clock(), message->machine_ids,
                                                     transition_state);

            for (auto machine_it = message->machine_ids.elements_begin();
                 machine_it != message->machine_ids.elements_end();
                 ++machine_it)
            {
                const int machine_id = *machine_it;
                Machine * machine = context->machines[machine_id];
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
                        if (context->current_switches.mark_switch_as_done(machine->id, message->new_pstate,
                                                                          all_switched_machines, context))
                        {
                            context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                                 std::to_string(message->new_pstate),
                                                                                 MSG_get_clock());
                        }
                    }
                    else if (machine->pstates[message->new_pstate] == PStateType::SLEEP_PSTATE)
                    {
                        machine->update_machine_state(MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING);
                        SwitchPStateProcessArguments * args = new SwitchPStateProcessArguments;
                        args->context = context;
                        args->machine_id = machine_id;
                        args->new_pstate = message->new_pstate;

                        string pname = "switch ON " + to_string(machine_id);
                        MSG_process_create(pname.c_str(), switch_off_machine_process, (void*)args, machine->host);

                        ++nb_switching_machines;
                    }
                    else
                        XBT_ERROR("Switching from a communication pstate to an invalid pstate on machine %d ('%s') : %d -> %d",
                                  machine->id, machine->name.c_str(), curr_pstate, message->new_pstate);
                }
                else if (machine->pstates[curr_pstate] == PStateType::SLEEP_PSTATE)
                {
                    xbt_assert(machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE,
                            "Switching from a sleep pstate to a non-computation pstate on machine %d ('%s') : %d -> %d, which is forbidden",
                            machine->id, machine->name.c_str(), curr_pstate, message->new_pstate);

                    machine->update_machine_state(MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING);
                    SwitchPStateProcessArguments * args = new SwitchPStateProcessArguments;
                    args->context = context;
                    args->machine_id = machine_id;
                    args->new_pstate = message->new_pstate;

                    string pname = "switch OFF " + to_string(machine_id);
                    MSG_process_create(pname.c_str(), switch_on_machine_process, (void*)args, machine->host);

                    ++nb_switching_machines;
                }
                else
                    XBT_ERROR("Machine %d ('%s') has an invalid pstate : %d", machine->id, machine->name.c_str(), curr_pstate);
            }

            if (context->trace_machine_states)
                context->machine_state_tracer.write_machine_states(MSG_get_clock());

        } break; // end of case PSTATE_MODIFICATION

        case IPMessageType::SCHED_EXECUTE_JOB:
        {
            xbt_assert(task_data->data != nullptr);
            ExecuteJobMessage * message = (ExecuteJobMessage *) task_data->data;
            SchedulingAllocation * allocation = message->allocation;

            Job * job = context->workloads.job_at(allocation->job_id);
            job->state = JobState::JOB_STATE_RUNNING;

            nb_running_jobs++;
            xbt_assert(nb_running_jobs <= nb_submitted_jobs);
            nb_scheduled_jobs++;
            xbt_assert(nb_scheduled_jobs <= nb_submitted_jobs);

            if (!context->allow_time_sharing)
            {
                for (auto machine_id_it = allocation->machine_ids.elements_begin(); machine_id_it != allocation->machine_ids.elements_end(); ++machine_id_it)
                {
                    int machine_id = *machine_id_it;
                    const Machine * machine = context->machines[machine_id];
                    (void) machine; // Avoids a warning if assertions are ignored
                    xbt_assert(machine->jobs_being_computed.empty(),
                               "Invalid job allocation: machine %d ('%s') is currently computing jobs (these ones:"
                               " {%s}) whereas space sharing is forbidden. Space sharing can be enabled via an option,"
                               " try --help to display the available options", machine->id, machine->name.c_str(),
                               machine->jobs_being_computed_as_string().c_str());
                }
            }

            if (context->energy_used)
            {
                // Check that every machine is in a computation pstate
                for (auto machine_id_it = allocation->machine_ids.elements_begin(); machine_id_it != allocation->machine_ids.elements_end(); ++machine_id_it)
                {
                    int machine_id = *machine_id_it;
                    Machine * machine = context->machines[machine_id];
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

            // Let's generate the hosts used by the job
            allocation->hosts.clear();
            allocation->hosts.reserve(allocation->machine_ids.size());
            int host_i = 0;
            for (auto machine_it = allocation->machine_ids.elements_begin(); machine_it != allocation->machine_ids.elements_end(); ++machine_it,++host_i)
            {
                int machine_id = *machine_it;
                allocation->hosts[host_i] = context->machines[machine_id]->host;
            }

            ExecuteJobProcessArguments * exec_args = new ExecuteJobProcessArguments;
            exec_args->context = context;
            exec_args->allocation = allocation;
            string pname = "job_" + job->id;
            msg_process_t process = MSG_process_create(pname.c_str(), execute_job_process,
                                                       (void*)exec_args,
                                                       context->machines[allocation->machine_ids.first_element()]->host);
            job->execution_processes.insert(process);
        } break; // end of case SCHED_ALLOCATION

        case IPMessageType::WAITING_DONE:
        {
            context->proto_writer->append_nop(MSG_get_clock());
            --nb_waiters;
        } break; // end of case WAITING_DONE

        case IPMessageType::SCHED_READY:
        {
            sched_ready = true;
        } break; // end of case SCHED_READY

        case IPMessageType::SCHED_WAIT_ANSWER:
        {
            SchedWaitAnswerMessage * message = new SchedWaitAnswerMessage;
            *message = *( (SchedWaitAnswerMessage *) task_data->data);

            //	  Submitter * submitter = origin_of_wait_queries.at({message->nb_resources,message->processing_time});
            dsend_message(message->submitter_name, IPMessageType::SCHED_WAIT_ANSWER, (void*) message);
            //	  origin_of_wait_queries.erase({message->nb_resources,message->processing_time});
        } break; // end of case SCHED_WAIT_ANSWER

        case IPMessageType::WAIT_QUERY:
        {
            //WaitQueryMessage * message = (WaitQueryMessage *) task_data->data;

            //	  XBT_INFO("received : %s , %s\n", to_string(message->nb_resources).c_str(), to_string(message->processing_time).c_str());
            xbt_assert(false, "Unimplemented! TODO");

            //Submitter * submitter = submitters.at(message->submitter_name);
            //origin_of_wait_queries[{message->nb_resources,message->processing_time}] = submitter;
        } break; // end of case WAIT_QUERY

        case IPMessageType::SWITCHED_ON:
        {
            xbt_assert(task_data->data != nullptr);
            SwitchONMessage * message = (SwitchONMessage *) task_data->data;

            xbt_assert(context->machines.exists(message->machine_id));
            Machine * machine = context->machines[message->machine_id];
            (void) machine; // Avoids a warning if assertions are ignored
            xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

            MachineRange all_switched_machines;
            if (context->current_switches.mark_switch_as_done(message->machine_id, message->new_pstate,
                                                              all_switched_machines, context))
            {
                if (context->trace_machine_states)
                    context->machine_state_tracer.write_machine_states(MSG_get_clock());

                context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                     std::to_string(message->new_pstate),
                                                                     MSG_get_clock());
            }

            --nb_switching_machines;
        } break; // end of case SWITCHED_ON

        case IPMessageType::SWITCHED_OFF:
        {
            xbt_assert(task_data->data != nullptr);
            SwitchOFFMessage * message = (SwitchOFFMessage *) task_data->data;

            xbt_assert(context->machines.exists(message->machine_id));
            Machine * machine = context->machines[message->machine_id];
            (void) machine; // Avoids a warning if assertions are ignored
            xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

            MachineRange all_switched_machines;
            if (context->current_switches.mark_switch_as_done(message->machine_id, message->new_pstate,
                                                              all_switched_machines, context))
            {
                if (context->trace_machine_states)
                    context->machine_state_tracer.write_machine_states(MSG_get_clock());

                context->proto_writer->append_resource_state_changed(all_switched_machines,
                                                                     std::to_string(message->new_pstate),
                                                                     MSG_get_clock());
            }

            --nb_switching_machines;
        } break; // end of case SWITCHED_ON

        case IPMessageType::SCHED_TELL_ME_ENERGY:
        {
            long double total_consumed_energy = context->machines.total_consumed_energy(context);
            context->proto_writer->append_query_reply_energy(total_consumed_energy, MSG_get_clock());
        } break; // end of case SCHED_TELL_ME_ENERGY

        case IPMessageType::SUBMITTER_CALLBACK:
        {
            xbt_assert(false, "The server received a SUBMITTER_CALLBACK message, which should not happen");
        } break; // end of case SUBMITTER_CALLBACK

        case IPMessageType::KILLING_DONE:
        {
            xbt_assert(task_data->data != nullptr);
            KillingDoneMessage * message = (KillingDoneMessage *) task_data->data;

            vector<string> job_ids_str;
            vector<string> really_killed_job_ids_str;
            job_ids_str.reserve(message->jobs_ids.size());

            for (const JobIdentifier & job_id : message->jobs_ids)
            {
                job_ids_str.push_back(job_id.to_string());

                const Job * job = context->workloads.job_at(job_id);
                if (job->state == JobState::JOB_STATE_COMPLETED_KILLED &&
                    job->kill_reason == "Killed from killer_process (probably requested by the decision process)")
                {
                    nb_running_jobs--;
                    xbt_assert(nb_running_jobs >= 0);
                    nb_completed_jobs++;
                    xbt_assert(nb_completed_jobs + nb_running_jobs <= nb_submitted_jobs);

                    really_killed_job_ids_str.push_back(job_id.to_string());
                }
            }

            XBT_INFO("Jobs {%s} have been killed (the following ones have REALLY been killed: {%s})",
                     boost::algorithm::join(job_ids_str, ",").c_str(),
                     boost::algorithm::join(really_killed_job_ids_str, ",").c_str());

            context->proto_writer->append_job_killed(job_ids_str, MSG_get_clock());
            --nb_killers;

            if (!all_jobs_submitted_and_completed &&
                nb_completed_jobs == nb_submitted_jobs &&
                nb_submitters_finished == nb_submitters)
            {
                all_jobs_submitted_and_completed = true;
                XBT_INFO("It seems that all jobs have been submitted and completed!");

                context->proto_writer->append_simulation_ends(MSG_get_clock());
            }
        } break; // end of case KILLING_DONE
        } // end of switch

        delete task_data;
        MSG_task_destroy(task_received);

        if (sched_ready && !context->proto_writer->is_empty() && !end_of_simulation_sent)
        {
            RequestReplyProcessArguments * req_rep_args = new RequestReplyProcessArguments;
            req_rep_args->context = context;
            req_rep_args->send_buffer = context->proto_writer->generate_current_message(MSG_get_clock());
            context->proto_writer->clear();

            MSG_process_create("Scheduler REQ-REP", request_reply_scheduler_process, (void*)req_rep_args, MSG_host_self());
            sched_ready = false;
            if (all_jobs_submitted_and_completed)
                end_of_simulation_sent = true;
        }

    } // end of while

    XBT_INFO("Simulation is finished!");
    bool simulation_is_completed = all_jobs_submitted_and_completed;
    xbt_assert(simulation_is_completed, "Left simulation loop, but the simulation does NOT seem finished...");

    delete args;
    return 0;
}

#include "server.hpp"

#include <string>

#include <boost/algorithm/string.hpp>

#include <simgrid/msg.h>

#include "context.hpp"
#include "ipp.hpp"
#include "network.hpp"
#include "jobs_execution.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(server, "server");

using namespace std;

int uds_server_process(int argc, char *argv[])
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
    int nb_running_jobs = 0;
    int nb_switching_machines = 0;
    const int protocol_version = 1;
    bool sched_ready = true;

    string send_buffer;

    // it may avoid the SG deadlock...
    while ((nb_submitters == 0) || (nb_submitters_finished < nb_submitters) ||
           (nb_completed_jobs < nb_submitted_jobs) || (!sched_ready) ||
           (nb_switching_machines > 0))
    {
        // Let's wait a message from a node or the request-reply process...
        msg_task_t task_received = NULL;
        IPMessage * task_data;
        MSG_task_receive(&(task_received), "server");
        task_data = (IPMessage *) MSG_task_get_data(task_received);

        XBT_INFO( "Server received a message of type %s:", ipMessageTypeToString(task_data->type).c_str());

        switch (task_data->type)
        {
        case IPMessageType::SUBMITTER_HELLO:
        {
            nb_submitters++;
            XBT_INFO( "New submitter said hello. Number of polite submitters: %d", nb_submitters);

        } break; // end of case SUBMITTER_HELLO

        case IPMessageType::SUBMITTER_BYE:
        {
            nb_submitters_finished++;
            XBT_INFO( "A submitted said goodbye. Number of finished submitters: %d", nb_submitters_finished);

        } break; // end of case SUBMITTER_BYE

        case IPMessageType::JOB_COMPLETED:
        {
            xbt_assert(task_data->data != nullptr);
            JobCompletedMessage * message = (JobCompletedMessage *) task_data->data;

            nb_running_jobs--;
            xbt_assert(nb_running_jobs >= 0);
            nb_completed_jobs++;
            Job * job = context->jobs[message->job_id];

            XBT_INFO( "Job %d COMPLETED. %d jobs completed so far", job->id, nb_completed_jobs);

            send_buffer += '|' + std::to_string(MSG_get_clock()) + ":C:" + std::to_string(job->id);
            XBT_DEBUG( "Message to send to scheduler: %s", send_buffer.c_str());

        } break; // end of case JOB_COMPLETED

        case IPMessageType::JOB_SUBMITTED:
        {
            xbt_assert(task_data->data != nullptr);
            JobSubmittedMessage * message = (JobSubmittedMessage *) task_data->data;

            nb_submitted_jobs++;
            Job * job = context->jobs[message->job_id];
            job->state = JobState::JOB_STATE_SUBMITTED;

            XBT_INFO( "Job %d SUBMITTED. %d jobs submitted so far", job->id, nb_submitted_jobs);
            send_buffer += "|" + std::to_string(MSG_get_clock()) + ":S:" + std::to_string(job->id);
            XBT_DEBUG( "Message to send to scheduler: '%s'", send_buffer.c_str());

        } break; // end of case JOB_SUBMITTED

        case IPMessageType::SCHED_REJECTION:
        {
            xbt_assert(task_data->data != nullptr);
            JobRejectedMessage * message = (JobRejectedMessage *) task_data->data;

            Job * job = context->jobs[message->job_id];
            job->state = JobState::JOB_STATE_REJECTED;
            nb_completed_jobs++;

            XBT_INFO( "Job %d has been rejected", job->id);
        } break; // end of case SCHED_REJECTION

        case IPMessageType::SCHED_NOP_ME_LATER:
        {
            xbt_assert(task_data->data != nullptr);
            NOPMeLaterMessage * message = (NOPMeLaterMessage *) task_data->data;

            WaiterProcessArguments * args = new WaiterProcessArguments;
            args->target_time = message->target_time;

            string pname = "waiter " + to_string(message->target_time);
            MSG_process_create(pname.c_str(), waiter_process, (void*) args, context->machines.masterMachine()->host);
        } break; // end of case SCHED_NOP_ME_LATER

        case IPMessageType::PSTATE_MODIFICATION:
        {
            xbt_assert(task_data->data != nullptr);
            PStateModificationMessage * message = (PStateModificationMessage *) task_data->data;

            Machine * machine = context->machines[message->machine];
            int curr_pstate = MSG_host_get_pstate(machine->host);

            if (machine->pstates[curr_pstate] == PStateType::COMPUTATION_PSTATE)
            {
                if (machine->pstates[message->new_pstate] == PStateType::COMPUTATION_PSTATE)
                {
                    XBT_INFO("Switching machine %d ('%s') pstate : %d -> %d.", machine->id,
                             machine->name.c_str(), curr_pstate, message->new_pstate);
                    MSG_host_set_pstate(machine->host, message->new_pstate);
                    context->pstate_tracer.add_pstate_change(MSG_get_clock(), machine->id, message->new_pstate);
                    xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

                    send_buffer += "|" + std::to_string(MSG_get_clock()) + ":p:" +
                                   std::to_string(machine->id) + "=" + std::to_string(message->new_pstate);
                    XBT_DEBUG( "Message to send to scheduler : '%s'", send_buffer.c_str());
                }
                else if (machine->pstates[message->new_pstate] == PStateType::SLEEP_PSTATE)
                {
                    machine->state = MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING;
                    SwitchPStateProcessArguments * args = new SwitchPStateProcessArguments;
                    args->context = context;
                    args->message = new PStateModificationMessage;
                    args->message->machine = message->machine;
                    args->message->new_pstate = message->new_pstate;

                    string pname = "switch ON " + to_string(message->machine);
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

                machine->state = MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING;
                SwitchPStateProcessArguments * args = new SwitchPStateProcessArguments;
                args->context = context;
                args->message = new PStateModificationMessage;
                args->message->machine = message->machine;
                args->message->new_pstate = message->new_pstate;

                string pname = "switch OFF " + to_string(message->machine);
                MSG_process_create(pname.c_str(), switch_on_machine_process, (void*)args, machine->host);

                ++nb_switching_machines;
            }
            else
                XBT_ERROR("Machine %d ('%s') has an invalid pstate : %d", machine->id, machine->name.c_str(), curr_pstate);

        } break; // end of case PSTATE_MODIFICATION

        case IPMessageType::SCHED_NOP:
        {
            XBT_INFO( "Nothing to do received.");
            if (nb_running_jobs == 0 && nb_scheduled_jobs < nb_submitted_jobs)
            {
                XBT_INFO( "Nothing to do whereas no job is running and that they are jobs waiting to be scheduled... This might cause a deadlock!");

                // Let us display the available jobs (to help the scheduler debugging)
                const std::map<int, Job *> & jobs = context->jobs.jobs();
                vector<string> submittedJobs;

                for (auto & mit : jobs)
                {
                    if (mit.second->state == JobState::JOB_STATE_SUBMITTED)
                        submittedJobs.push_back(std::to_string(mit.second->id));
                }

                string submittedJobsString = boost::algorithm::join(submittedJobs, ", ");

                XBT_INFO( "The available jobs are [%s]", submittedJobsString.c_str());
            }

        } break; // end of case SCHED_NOP

        case IPMessageType::SCHED_ALLOCATION:
        {
            xbt_assert(task_data->data != nullptr);
            SchedulingAllocationMessage * message = (SchedulingAllocationMessage *) task_data->data;

            for (const auto & allocation : message->allocations)
            {
                Job * job = context->jobs[allocation.job_id];
                job->state = JobState::JOB_STATE_RUNNING;

                nb_running_jobs++;
                xbt_assert(nb_running_jobs <= nb_submitted_jobs);
                nb_scheduled_jobs++;
                xbt_assert(nb_scheduled_jobs <= nb_submitted_jobs);

                if (!context->allow_space_sharing)
                {
                    for (const int & machineID : allocation.machine_ids)
                    {
                        const Machine * machine = context->machines[machineID];
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
                    for (const int & machineID : allocation.machine_ids)
                    {
                        Machine * machine = context->machines[machineID];
                        int ps = MSG_host_get_pstate(machine->host);
                        xbt_assert(machine->has_pstate(ps));
                        xbt_assert(machine->pstates[ps] == PStateType::COMPUTATION_PSTATE,
                                   "Invalid job allocation: machine %d ('%s') is not in a computation pstate (ps=%d)",
                                   machine->id, machine->name.c_str(), ps);
                        xbt_assert(machine->state == MachineState::COMPUTING || machine->state == MachineState::IDLE,
                                   "Invalid job allocation: machine %d ('%s') cannot compute jobs now (the machine is"
                                   " neither computing nor being idle)", machine->id, machine->name.c_str());
                    }

                }

                ExecuteJobProcessArguments * exec_args = new ExecuteJobProcessArguments;
                exec_args->context = context;
                exec_args->allocation = allocation;
                string pname = "job" + to_string(job->id);
                MSG_process_create(pname.c_str(), execute_job_process, (void*)exec_args, context->machines[allocation.machine_ids[0]]->host);
            }

        } break; // end of case SCHED_ALLOCATION

        case IPMessageType::WAITING_DONE:
        {
            send_buffer += "|" + std::to_string(MSG_get_clock()) + ":N";
            XBT_DEBUG("Message to send to scheduler: '%s'", send_buffer.c_str());
        } break; // end of case WAITING_DONE

        case IPMessageType::SCHED_READY:
        {
            sched_ready = true;

        } break; // end of case SCHED_READY

        case IPMessageType::SWITCHED_ON:
        {
            xbt_assert(task_data->data != nullptr);
            PStateModificationMessage * message = (PStateModificationMessage *) task_data->data;

            xbt_assert(context->machines.exists(message->machine));
            Machine * machine = context->machines[message->machine];
            xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

            send_buffer += "|" + std::to_string(MSG_get_clock()) + ":p:" +
                           std::to_string(machine->id) + "=" + std::to_string(message->new_pstate);
            XBT_DEBUG("Message to send to scheduler : '%s'", send_buffer.c_str());

            --nb_switching_machines;
        } break; // end of case SWITCHED_ON

        case IPMessageType::SWITCHED_OFF:
        {
            xbt_assert(task_data->data != nullptr);
            PStateModificationMessage * message = (PStateModificationMessage *) task_data->data;

            xbt_assert(context->machines.exists(message->machine));
            Machine * machine = context->machines[message->machine];
            xbt_assert(MSG_host_get_pstate(machine->host) == message->new_pstate);

            send_buffer += "|" + std::to_string(MSG_get_clock()) + ":p:" +
                           std::to_string(machine->id) + "=" + std::to_string(message->new_pstate);
            XBT_DEBUG("Message to send to scheduler : '%s'", send_buffer.c_str());

            --nb_switching_machines;
        } break; // end of case SWITCHED_ON

        case IPMessageType::SCHED_TELL_ME_ENERGY:
        {
            long double total_consumed_energy = context->machines.total_consumed_energy(context);

            send_buffer += "|" + std::to_string(MSG_get_clock()) + ":e:" +
                           std::to_string(total_consumed_energy);
            XBT_DEBUG("Message to send to scheduler : '%s'", send_buffer.c_str());
        }
        } // end of switch

        delete task_data;
        MSG_task_destroy(task_received);

        if (sched_ready && !send_buffer.empty())
        {
            RequestReplyProcessArguments * req_rep_args = new RequestReplyProcessArguments;
            req_rep_args->context = context;
            req_rep_args->send_buffer = to_string(protocol_version) + ":" + to_string(MSG_get_clock()) + send_buffer;
            send_buffer.clear();

            MSG_process_create("Scheduler REQ-REP", request_reply_scheduler_process, (void*)req_rep_args, MSG_host_self());
            sched_ready = false;
        }

    } // end of while

    XBT_INFO( "All jobs completed!");

    delete args;
    return 0;
}

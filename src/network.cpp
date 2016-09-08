/**
 * @file network.cpp
 * @brief Contains network-related classes and functions
 */

#include "network.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include <simgrid/msg.h>

#include "context.hpp"
#include "ipp.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(network, "network"); //!< Logging

using namespace std;

UnixDomainSocket::UnixDomainSocket()
{
}

UnixDomainSocket::UnixDomainSocket(const std::string & filename) : UnixDomainSocket()
{
    create_socket(filename);
}

UnixDomainSocket::~UnixDomainSocket()
{
    close();
}

void UnixDomainSocket::create_socket(const string & filename)
{
    XBT_INFO("Creating UDS socket on '%s'", filename.c_str());

    _server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    xbt_assert(_server_socket != -1, "Impossible to create socket");

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, filename.c_str(), sizeof(addr.sun_path)-1);

    unlink(filename.c_str());

    int ret = bind(_server_socket, (struct sockaddr*)&addr, sizeof(addr));
    xbt_assert(ret != -1, "Impossible to bind socket");

    ret = listen(_server_socket, 1);
    xbt_assert(ret != -1, "Impossible to listen on socket");
}

void UnixDomainSocket::accept_pending_connection()
{
    XBT_INFO("Waiting for an incoming connection...");
    _client_socket = accept(_server_socket, NULL, NULL);
    xbt_assert(_client_socket != -1, "Impossible to accept on socket");
    XBT_INFO("Connected!");
}

void UnixDomainSocket::close()
{
    if (_client_socket != -1)
    {
        ::close(_client_socket);
        _client_socket = -1;
    }

    if (_server_socket != -1)
    {
        ::close(_server_socket);
        _server_socket = -1;
    }
}

string UnixDomainSocket::receive()
{
    xbt_assert(_client_socket != -1, "Bad UnixDomainSocket::receive call: the client socket does not exist");

    string msg;
    uint32_t message_size;
    uint32_t nb_bytes_read = 0;
    int ret;

    /* Since messages can be split up to 1 byte messages, the following code
       continues to read until 4 bytes are read. If one read returns a
       non-strictly-positive value, we assume something wrong happened
       and we can close the socket.
    */

    while (nb_bytes_read < 4)
    {
        ret = read(_client_socket, (void*)(((char*)&message_size)+nb_bytes_read), 4 - nb_bytes_read);

        if (ret < 1)
        {
            ::close(_client_socket);
            _client_socket = -1;
            throw std::runtime_error("Cannot read on socket. Closed by remote?");
        }
        nb_bytes_read += ret;
    }

    xbt_assert(message_size > 0, "Invalid message received (size=%d)", message_size);
    msg.resize(message_size);

    // The message content is then read, following the same logic
    nb_bytes_read = 0;
    while (nb_bytes_read < message_size)
    {
        ret = read(_client_socket, (void*)(msg.data()+nb_bytes_read), message_size - nb_bytes_read);
        if (ret < 1)
        {
            ::close(_client_socket);
            _client_socket = -1;
            throw std::runtime_error("Cannot read on socket. Closed by remote?");
        }
        nb_bytes_read += ret;
    }
    XBT_INFO("Received '%s'", msg.c_str());

    return msg;
}

void UnixDomainSocket::send(const string & message)
{
    xbt_assert(_client_socket != -1, "Bad UnixDomainSocket::send call: the client socket does not exist");

    uint32_t message_size = message.size();
    XBT_INFO("Sending '%s'", message.c_str());
    write(_client_socket, &message_size, 4);
    write(_client_socket, (void*)message.c_str(), message_size);
}

int request_reply_scheduler_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    RequestReplyProcessArguments * args = (RequestReplyProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;

    char sendDateAsString[16];
    sprintf(sendDateAsString, "%f", MSG_get_clock());

    char *sendBuf = (char*) args->send_buffer.c_str();
    XBT_DEBUG("Buffer received in REQ-REP: '%s'", sendBuf);

    context->socket.send(sendBuf);

    auto start = chrono::steady_clock::now();
    string message_received;

    try
    {
         message_received = context->socket.receive();
    }
    catch(const std::runtime_error & error)
    {
        XBT_INFO("Runtime error received: %s", error.what());
        XBT_INFO("Flushing output files...");

        if (context->trace_schedule)
            context->paje_tracer.finalize(context, MSG_get_clock());
        exportScheduleToCSV(context->export_prefix + "_schedule.csv", context);
        exportJobsToCSV(context->export_prefix + "_jobs.csv", context);

        XBT_INFO("Output files flushed. Aborting execution now.");
        throw runtime_error("Execution aborted (connection broken)");
    }

    auto end = chrono::steady_clock::now();
    long long elapsed_microseconds = chrono::duration <double, micro> (end - start).count();
    context->microseconds_used_by_scheduler += elapsed_microseconds;

    /*
        The kind of message that is expected to be received is the following:
        0:25.836709|15.000015:J:1=1,2,0,3;2=3|20.836694:N|25.836709:J:2=0,2,1,3

        The top-level separator is '|'.
            The first part is always 0:MESSAGE_SENDING_DATE
            There are then a variable number of parts (> 1). Those parts are called events.
            They are composed of the form EVENT_DATE:STAMP[:STAMP_RELATED_CONTENT]. Stamps are 1 char long.
            Reading the events from left to right, their event dates must be non-decreasing.
            The following stamp exist at the moment :
                N : do nothing. Content : none.
                J : allocation of a static job (described in the JSON file). Content : jobID1=mID1_1,mID1_2,...,mID1_n[;jobID2=MID2_1,...MID2_n[;...]]
                P : changes the pstate of a machine. Content : mID=pstate
    */

    // Let us split the message by '|'.

    vector<string> events;
    boost::split(events, message_received, boost::is_any_of("|"), boost::token_compress_on);
    xbt_assert(events.size() >= 2, "Invalid message received ('%s'): it should be composed of at least 2 parts separated by a '|'", message_received.c_str());

    double previousDate = std::stod (sendDateAsString);

    for (unsigned int eventI = 1; eventI < events.size(); ++eventI)
    {
        const std::string & event_string = events[eventI];
        // Let us split the event by ':'.
        vector<string> parts2;
        boost::split(parts2, event_string, boost::is_any_of(":"), boost::token_compress_on);
        xbt_assert(parts2.size() >= 2, "Invalid event received ('%s'): it should be composed of at least 2 parts separated by a ':'", event_string.c_str());
        xbt_assert(parts2[1].size() == 1, "Invalid event received ('%s'): network stamp ('%s') should be of length 1", event_string.c_str(), parts2[1].c_str());

        double date = std::stod(parts2[0]);
        NetworkStamp received_stamp = (NetworkStamp) parts2[1][0];

        xbt_assert(date >= previousDate, "Invalid event received ('%s'): its date (%lf) cannot be before the previous event date (%lf)",
                   event_string.c_str(), date, previousDate);

        // Let us wait until the event occurrence
        MSG_process_sleep(std::max(0.0, date - previousDate));
        previousDate = date;

        switch(received_stamp)
        {
            case NOP:
            {
                xbt_assert(parts2.size() == 2, "Invalid event received ('%s'): NOP messages must be composed of 2 parts separated by ':'", event_string.c_str());
                dsend_message("server", IPMessageType::SCHED_NOP);
            } break; // End of case received_stamp == NOP

            case NOP_ME_LATER:
            {
                xbt_assert(parts2.size() == 3, "Invalid event received ('%s'): NOP_ME_LATER messages must be composed of 3 parts separated by ':'", event_string.c_str());

                NOPMeLaterMessage * message = new NOPMeLaterMessage;
                message->target_time = std::stod(parts2[2]);
                if (message->target_time < MSG_get_clock())
                    XBT_WARN("Event '%s' tells to wait until time %g but it is already reached", event_string.c_str(), message->target_time);

                dsend_message("server", IPMessageType::SCHED_NOP_ME_LATER, (void*) message);
            } break; // End of case received_stamp == NOP_ME_LATER

            case STATIC_JOB_ALLOCATION:
            {
                xbt_assert(parts2.size() == 3, "Invalid event received ('%s'): static job allocations must be composed of 3 parts separated by ':'",
                           event_string.c_str());

                // Let us create the message which will be sent to the server.
                SchedulingAllocationMessage * message = new SchedulingAllocationMessage;

                // Since allocations can be done on several jobs at once, let us split the content again, by ';' this time.
                vector<string> allocations;
                boost::split(allocations, parts2[2], boost::is_any_of(";"), boost::token_compress_on);

                for (const std::string & allocation_string : allocations)
                {
                    // Each allocation is written in the form of wload!jobID=machineID1,machineID2,...,machineIDn
                    // Each machineIDk can either be a single machine or a closed interval machineIDa-machineIDb
                    vector<string> allocation_parts;
                    boost::split(allocation_parts, allocation_string, boost::is_any_of("="), boost::token_compress_on);
                    xbt_assert(allocation_parts.size() == 2, "Invalid job allocation received ('%s'): it must be composed of two parts separated by a '='",
                               allocation_string.c_str());

                    SchedulingAllocation * alloc = new SchedulingAllocation;

                    // Let's retrieve the job identifier
                    if (!identify_job_from_string(context, allocation_parts[0], alloc->job_id))
                    {
                        xbt_assert(false, "Invalid job allocation received ('%s'): The job identifier '%s' is not valid. "
                                   "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                                   "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                                   "Furthermore, the corresponding job must exist.",
                                   allocation_string.c_str(), allocation_parts[0].c_str());
                    }

                    Job * job = context->workloads.job_at(alloc->job_id);
                    (void) job; // Avoids a warning if assertions are ignored
		    XBT_INFO("JOB_STATE (job %d): %d ('0' means 'not submitted')", 
                                      job->number, job->state);

                    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
                               "Invalid static job allocation received ('%s') : the job %d state indicates it cannot be executed now",
                               allocation_string.c_str(), job->number);

                    // In order to get the machines, let us do a split by ','!
                    vector<string> allocation_machines;
                    boost::split(allocation_machines, allocation_parts[1], boost::is_any_of(","), boost::token_compress_on);

                    alloc->hosts.clear();
                    alloc->hosts.reserve(alloc->machine_ids.size());
                    alloc->machine_ids.clear();
                    for (unsigned int i = 0; i < allocation_machines.size(); ++i)
                    {
                        // Since each machineIDk can either be a single machine or a closed interval, let's try to split on '-'
                        vector<string> interval_parts;
                        boost::split(interval_parts, allocation_machines[i], boost::is_any_of("-"), boost::token_compress_on);
                        xbt_assert(interval_parts.size() >= 1 && interval_parts.size() <= 2,
                                   "Invalid job allocation received ('%s'): the MIDk '%s' should either be a single machine ID"
                                   " (syntax: MID to represent the machine ID MID) or a closed interval (syntax: MIDa-MIDb to represent"
                                   " the machine interval [MIDA,MIDb])", allocation_string.c_str(), allocation_machines[i].c_str());

                        if (interval_parts.size() == 1)
                        {
                            int machine_id = std::stoi(interval_parts[0]);
                            xbt_assert(context->machines.exists(machine_id), "Invalid job allocation received ('%s'): the machine %d does not exist",
                                       allocation_string.c_str(), machine_id);
                            alloc->machine_ids.insert(machine_id);
                            alloc->hosts.push_back(context->machines[machine_id]->host);
                            xbt_assert((int)alloc->hosts.size() <= job->required_nb_res,
                                       "Invalid job allocation received ('%s'): the job %d size is %d but at least %lu machines were allocated",
                                       allocation_string.c_str(), job->number, job->required_nb_res, alloc->hosts.size());
                        }
                        else
                        {
                            int machineIDa = std::stoi(interval_parts[0]);
                            int machineIDb = std::stoi(interval_parts[1]);

                            xbt_assert(machineIDa <= machineIDb, "Invalid job allocation received ('%s'): the MIDk '%s' is composed of two"
                                      " parts (1:%d and 2:%d) but the first value must be lesser than or equal to the second one.", allocation_string.c_str(),
                                      allocation_machines[i].c_str(), machineIDa, machineIDb);

                            for (int machine_id = machineIDa; machine_id <= machineIDb; ++machine_id)
                            {
                                xbt_assert(context->machines.exists(machine_id), "Invalid job allocation received ('%s'): the machine %d does not exist",
                                           allocation_string.c_str(), machine_id);

                                alloc->hosts.push_back(context->machines[machine_id]->host);
                                xbt_assert((int)alloc->hosts.size() <= job->required_nb_res,
                                           "Invalid static job allocation received ('%s'): the job %d size is %d but at least %lu machines were allocated",
                                           allocation_string.c_str(), job->number, job->required_nb_res, alloc->hosts.size());
                            }

                            alloc->machine_ids.insert(MachineRange::ClosedInterval(machineIDa, machineIDb));
                        }
                    }

                    // Let the number of allocated machines be checked
                    xbt_assert((int)alloc->machine_ids.size() == job->required_nb_res,
                               "Invalid job allocation received ('%s'): the job %d size is %d but %u machines were allocated (%s)",
                               allocation_string.c_str(), job->number, job->required_nb_res, alloc->machine_ids.size(),
                               alloc->machine_ids.to_string_brackets().c_str());

                    message->allocations.push_back(alloc);
                }

                send_message("server", IPMessageType::SCHED_ALLOCATION, (void*) message);

            } break; // End of case received_stamp == STATIC_JOB_ALLOCATION

            case JOB_REJECTION:
            {
                xbt_assert(parts2.size() == 3, "Invalid event received ('%s'): job rejections must be composed of 3 parts separated by ':'",
                           event_string.c_str());

                // Let us create the message which will be sent to the server.
                JobRejectedMessage * message = new JobRejectedMessage;

                // Let's retrieve the workload_name and the job_id from the job identifier
                const std::string & job_identifier_string = parts2[2];
                if (!identify_job_from_string(context, job_identifier_string, message->job_id))
                {
                    xbt_assert(false, "Invalid job rejection received: The job identifier '%s' is not valid. "
                               "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                               "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                               "Furthermore, the corresponding job must exist.",
                               job_identifier_string.c_str());
                }


                Job * job = context->workloads.job_at(message->job_id);
                (void) job; // Avoids a warning if assertions are ignored
                xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED, "Invalid event received ('%s'): job %d cannot be"
                          " rejected now. For being rejected, a job must be submitted and not allocated yet.",
                           event_string.c_str(), job->number);

                send_message("server", IPMessageType::SCHED_REJECTION, (void*) message);

            } break; // End of case received_stamp == JOB_REJECTION

            case PSTATE_SET:
            {
                xbt_assert(parts2.size() == 3, "Invalid event received ('%s'): pstate modifications must be composed of 3 parts separated by ':'",
                           event_string.c_str());
                xbt_assert(context->energy_used, "A pstate modification message has been received whereas energy is not currently used by Batsim."
                           " You can use the energy plugin of Batsim via a command-line option, try --help to display those options.");

                PStateModificationMessage * message = new PStateModificationMessage;

                // Let the content part of the message splitted by "="
                vector<string> parts3;
                boost::split(parts3, parts2[2], boost::is_any_of("="), boost::token_compress_on);
                xbt_assert(parts3.size() == 2, "Invalid event received ('%s'): invalid pstate modification message content ('%s'): it must be"
                           " formated like M=P where M is a machine range (1,2,3,6,7 or 1-3,6-7 for example) and P a pstate number of machines M",
                           event_string.c_str(), parts2[2].c_str());

                int pstate = std::stoi(parts3[1]);

                string error_prefix = "Invalid pstate modification message content ('" + event_string + "')";
                message->machine_ids = MachineRange::from_string_hyphen(parts3[0], ",", "-", error_prefix);

                for (auto machine_it = message->machine_ids.elements_begin(); machine_it != message->machine_ids.elements_end(); ++machine_it)
                {
                    int machine_id = *machine_it;
                    xbt_assert(context->machines.exists(machine_id), "%s: the machine %d does not exist", error_prefix.c_str(), machine_id);

                    Machine * machine = context->machines[machine_id];
                    xbt_assert(machine->state == MachineState::IDLE || machine->state == MachineState::SLEEPING,
                               "%s: machine %d's pstate can only be changed while the"
                               " machine is idle or sleeping, which is not the case now", error_prefix.c_str(), machine_id);
                    xbt_assert(machine->has_pstate(pstate), "%s: machine %d has no pstate %d", error_prefix.c_str(), machine_id, pstate);

                    int current_pstate = MSG_host_get_pstate(machine->host);
                    xbt_assert(machine->has_pstate(current_pstate));

                    if (machine->pstates[current_pstate] == PStateType::COMPUTATION_PSTATE)
                    {
                        xbt_assert(machine->pstates[pstate] == PStateType::COMPUTATION_PSTATE ||
                                   machine->pstates[pstate] == PStateType::SLEEP_PSTATE, "%s: asked to switch machine %d ('%s') from"
                                   " pstate %d (a computation one) to pstate %d (not a computation pstate nor a sleep pstate), which is forbidden",
                                   error_prefix.c_str(), machine_id, machine->name.c_str(), current_pstate, pstate);
                    }
                    else if (machine->pstates[current_pstate] == PStateType::SLEEP_PSTATE)
                    {
                        xbt_assert(machine->pstates[pstate] == PStateType::COMPUTATION_PSTATE, "%s: asked to switch machine %d ('%s') from"
                                   " pstate %d (a sleep pstate) to pstate %d (not a computation pstate), which is forbidden",
                                   error_prefix.c_str(), machine_id, machine->name.c_str(), current_pstate, pstate);
                    }
                }

                message->new_pstate = pstate;
                send_message("server", IPMessageType::PSTATE_MODIFICATION, (void*) message);

            } break; // End of case received_stamp == PSTATE_SET

            case TELL_ME_CONSUMED_ENERGY:
            {
                xbt_assert(parts2.size() == 2, "Invalid event received ('%s'): messages to ask the consumed energy must be composed of 2 parts separated by ':'",
                           event_string.c_str());
                xbt_assert(context->energy_used, "A message to ask the consumed energy has been received whereas energy is not currently used by Batsim."
                           " You can use the energy plugin of Batsim via a command-line option, try --help to display those options.");
                send_message("server", IPMessageType::SCHED_TELL_ME_ENERGY);
            } break; // End of case received_stamp == TELL_ME_CONSUMED_ENERGY

            default:
            {
                xbt_die("Invalid event received ('%s') : unhandled network stamp received ('%c')", event_string.c_str(), received_stamp);
            }
        } // end of job type switch
    } // end of events traversal

    send_message("server", IPMessageType::SCHED_READY);
    delete args;
    return 0;
}

std::string absolute_filename(const std::string & filename)
{
    xbt_assert(filename.length() > 0);

    // Let's assume filenames starting by "/" are absolute.
    if (filename[0] == '/')
        return filename;

    char cwd_buf[PATH_MAX];
    char * getcwd_ret = getcwd(cwd_buf, PATH_MAX);
    xbt_assert(getcwd_ret == cwd_buf, "getcwd failed");

    return string(getcwd_ret) + '/' + filename;
}

bool identify_job_from_string(BatsimContext * context,
                              const std::string & job_identifier_string,
                              JobIdentifier & job_id)
{
    // Let's split the job_identifier by '!'
    vector<string> job_identifier_parts;
    boost::split(job_identifier_parts, job_identifier_string, boost::is_any_of("!"), boost::token_compress_on);

    if (job_identifier_parts.size() == 1)
    {
	XBT_INFO("WARNING: Job ID is not of format WORKLOAD!NUMBER... assuming static!");
        job_id.workload_name = "static";
        job_id.job_number = std::stoi(job_identifier_parts[0]);
    }
    else if (job_identifier_parts.size() == 2)
    {
        job_id.workload_name = job_identifier_parts[0];
        job_id.job_number = std::stoi(job_identifier_parts[1]);
    }
    else
        return false;

    return context->workloads.job_exists(job_id);
}

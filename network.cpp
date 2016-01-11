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

XBT_LOG_NEW_DEFAULT_CATEGORY(network, "network");

using namespace std;

UnixDomainSocket::UnixDomainSocket()
{
    _server_socket = -1;
    _client_socket = -1;
}

UnixDomainSocket::UnixDomainSocket(const string & filename) : UnixDomainSocket()
{
    create_socket(filename);
}

UnixDomainSocket::~UnixDomainSocket()
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

string UnixDomainSocket::receive()
{
    string msg;
    int32_t message_size;
    int nb_bytes_read = 0;
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
    int32_t message_size = message.size();
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
    string message_received = context->socket.receive();
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
                send_message("server", IPMessageType::SCHED_NOP);
            } break; // End of case received_stamp == NOP

            case NOP_ME_LATER:
            {
                xbt_assert(parts2.size() == 3, "Invalid event received ('%s'): NOP_ME_LATER messages must be composed of 3 parts separated by ':'", event_string.c_str());

                NOPMeLaterMessage * message = new NOPMeLaterMessage;
                message->target_time = std::stod(parts2[2]);
                if (message->target_time < MSG_get_clock())
                    XBT_WARN("Event '%s' tells to wait until time %g but it is already reached", event_string.c_str(), message->target_time);

                send_message("server", IPMessageType::SCHED_NOP_ME_LATER, (void*) message);
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
                    // Each allocation is written in the form of jobID=machineID1,machineID2,...,machineIDn
                    vector<string> allocation_parts;
                    boost::split(allocation_parts, allocation_string, boost::is_any_of("="), boost::token_compress_on);
                    xbt_assert(allocation_parts.size() == 2, "Invalid static job allocation received ('%s'): it must be composed of two parts separated by a '='",
                               allocation_string.c_str());

                    SchedulingAllocation alloc;
                    alloc.job_id = std::stoi(allocation_parts[0]);
                    xbt_assert(context->jobs.exists(alloc.job_id), "Invalid static job allocation received ('%s'): the job %d does not exist",
                               allocation_string.c_str(), alloc.job_id);
                    Job * job = context->jobs[alloc.job_id];
                    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
                               "Invalid static job allocation received ('%s') : the job %d state indicates it cannot be executed now",
                               allocation_string.c_str(), job->id);

                    // In order to get the machines, let us do a final split by ','!
                    vector<string> allocation_machines;
                    boost::split(allocation_machines, allocation_parts[1], boost::is_any_of(","), boost::token_compress_on);
                    xbt_assert((int)allocation_machines.size() == job->required_nb_res,
                               "Invalid static job allocation received ('%s'): the job %d size is %d but %lu machines were allocated",
                               allocation_string.c_str(), job->id, job->required_nb_res, allocation_machines.size());

                    alloc.machine_ids.resize(allocation_machines.size());
                    alloc.hosts.resize(allocation_machines.size());
                    for (unsigned int i = 0; i < allocation_machines.size(); ++i)
                    {
                        int machineID = std::stoi(allocation_machines[i]);
                        xbt_assert(context->machines.exists(machineID), "Invalid static job allocation received ('%s'): the machine %d does not exist",
                                   allocation_string.c_str(), machineID);
                        alloc.machine_ids[i] = machineID;
                        alloc.hosts[i] = context->machines[machineID]->host;
                    }

                    // Let us sort the allocation, to detect easily whether all machines are different or not
                    vector<int> sorted_machine_ids = alloc.machine_ids;
                    std::sort(sorted_machine_ids.begin(), sorted_machine_ids.end());

                    bool all_different = true;
                    for (unsigned int i = 1; i < sorted_machine_ids.size(); ++i)
                    {
                        if (sorted_machine_ids[i-1] == sorted_machine_ids[i])
                        {
                            all_different = false;
                            break;
                        }
                    }

                    xbt_assert(all_different, "Invalid static job allocation received ('%s'): all machines are not different", allocation_string.c_str());

                    message->allocations.push_back(alloc);
                }

                send_message("server", IPMessageType::SCHED_ALLOCATION, (void*) message);

            } break; // End of case received_stamp == STATIC_JOB_ALLOCATION

            case JOB_REJECTION:
            {
                xbt_assert(parts2.size() == 3, "Invalid event received ('%s'): static job rejections must be composed of 3 parts separated by ':'",
                           event_string.c_str());

                // Let us create the message which will be sent to the server.
                JobRejectedMessage * message = new JobRejectedMessage;
                message->job_id = std::stoi(parts2[2]);

                xbt_assert(context->jobs.exists(message->job_id), "Invalid event received ('%s'): job %d does not exist",
                           event_string.c_str(), message->job_id);
                Job * job = context->jobs[message->job_id];
                xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED, "Invalid event received ('%s'): job %d cannot be"
                          " rejected now. For being rejected, a job must be submitted and not allocated yet.",
                           event_string.c_str(), job->id);

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
                           " of type M=P where M is a machine number and P a pstate number of machine M", event_string.c_str(), parts2[2].c_str());

                int machineID = std::stoi(parts3[0]);
                int pstate = std::stoi(parts3[1]);

                xbt_assert(context->machines.exists(machineID), "Invalid event received ('%s'): machine %d does not exist", event_string.c_str(), machineID);
                Machine * machine = context->machines[machineID];
                xbt_assert(machine->state == MachineState::IDLE || machine->state == MachineState::SLEEPING,
                           "Invalid event received ('%s'): machine %d's pstate can only be changed while the"
                           " machine is idle or sleeping, which is not the case now.", event_string.c_str(), machineID);
                xbt_assert(context->machines[machineID]->has_pstate(pstate), "Invalid event received ('%s'): machine %d has no pstate %d", event_string.c_str(), machineID, pstate);

                int current_pstate = MSG_host_get_pstate(machine->host);
                xbt_assert(machine->has_pstate(current_pstate));

                if (machine->pstates[current_pstate] == PStateType::COMPUTATION_PSTATE)
                {
                    xbt_assert(machine->pstates[pstate] == PStateType::COMPUTATION_PSTATE ||
                               machine->pstates[pstate] == PStateType::SLEEP_PSTATE, "Invalid event received ('%s'): asked to switch machine %d ('%s') from"
                               " pstate %d (a computation one) to pstate %d (not a computation pstate nor a sleep pstate), which is forbidden",
                               event_string.c_str(), machineID, machine->name.c_str(), current_pstate, pstate);
                }
                else if (machine->pstates[current_pstate] == PStateType::SLEEP_PSTATE)
                {
                    xbt_assert(machine->pstates[pstate] == PStateType::COMPUTATION_PSTATE, "Invalid event received ('%s'): asked to switch machine %d ('%s') from"
                               " pstate %d (a sleep pstate) to pstate %d (not a computation pstate), which is forbidden",
                               event_string.c_str(), machineID, machine->name.c_str(), current_pstate, pstate);
                }

                message->machine = machineID;
                message->new_pstate = pstate;

                send_message("server", IPMessageType::PSTATE_MODIFICATION, (void*) message);

            } break; // End of case received_stamp == PSTATE_SET

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

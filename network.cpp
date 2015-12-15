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
#include "jobs_execution.hpp"

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
    XBT_INFO("message_size = %d", message_size);

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
    XBT_CINFO(network, "Received '%s'", msg.c_str());

    return msg;
}

void UnixDomainSocket::send(const string & message)
{
    int32_t message_size = message.size();
    XBT_CINFO(network, "Sending '%s'", message.c_str());
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
    XBT_INFO("Buffer received in REQ-REP: '%s'", sendBuf);

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
    */

    // Let us split the message by '|'.
    vector<string> events;
    boost::split(events, message_received, boost::is_any_of("|"), boost::token_compress_on);
    xbt_assert(events.size() >= 2, "Invalid message received ('%s'): it should be composed of at least 2 parts separated by a '|'", message_received.c_str());

    double previousDate = atof(sendDateAsString);

    for (unsigned int eventI = 1; eventI < events.size(); ++eventI)
    {
        const std::string & event_string = events[eventI];
        // Let us split the event by ':'.
        vector<string> parts2;
        boost::split(parts2, event_string, boost::is_any_of(":"), boost::token_compress_on);
        xbt_assert(parts2.size() >= 2, "Invalid event received ('%s'): it should be composed of at least 2 parts separated by a ':'", event_string.c_str());
        xbt_assert(parts2[1].size() == 1, "Invalid event received ('%s'): network stamp ('%s') should be of length 1", event_string.c_str(), parts2[1].c_str());

        double date = std::stof(parts2[0]);
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
                send_message("server", IPMessageType::SCHED_NOP);
            } break; // End of case received_stamp == NOP
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
                    std::sort(alloc.machine_ids.begin(), alloc.machine_ids.end());

                    bool all_different = true;
                    for (unsigned int i = 1; i < alloc.machine_ids.size(); ++i)
                    {
                        if (alloc.machine_ids[i-1] == alloc.machine_ids[i])
                        {
                            all_different = false;
                            break;
                        }
                    }

                    xbt_assert(all_different, "Invalid static job allocation received ('%s') : all machines are not different", allocation_string.c_str());

                    message->allocations.push_back(alloc);
                }

                send_message("server", IPMessageType::SCHED_ALLOCATION, (void*) message);

            } break; // End of case received_stamp == STATIC_JOB_ALLOCATION
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
    bool sched_ready = true;

    string send_buffer;

    // it may avoid the SG deadlock...
    while ((nb_submitters == 0) || (nb_submitters_finished < nb_submitters) ||
           (nb_completed_jobs < nb_submitted_jobs) || !sched_ready)
    {
        // Let's wait a message from a node or the request-reply process...
        msg_task_t task_received = NULL;
        IPMessage * task_data;
        MSG_task_receive(&(task_received), "server");
        task_data = (IPMessage *) MSG_task_get_data(task_received);

        XBT_INFO("Server received a message of type %s:", ipMessageTypeToString(task_data->type).c_str());

        switch (task_data->type)
        {
        case IPMessageType::SUBMITTER_HELLO:
        {
            nb_submitters++;
            XBT_INFO("New submitter said hello. Number of polite submitters: %d", nb_submitters);
            break;
        } // end of case SUBMITTER_HELLO
        case IPMessageType::SUBMITTER_BYE:
        {
            nb_submitters_finished++;
            XBT_INFO("A submitted said goodbye. Number of finished submitters: %d", nb_submitters_finished);
            break;
        } // end of case SUBMITTER_BYE
        case IPMessageType::JOB_COMPLETED:
        {
            xbt_assert(task_data->data != nullptr);
            JobCompletedMessage * message = (JobCompletedMessage *) task_data->data;

            nb_running_jobs--;
            xbt_assert(nb_running_jobs >= 0);
            nb_completed_jobs++;
            Job * job = context->jobs[message->job_id];

            XBT_INFO("Job %d COMPLETED. %d jobs completed so far", job->id, nb_completed_jobs);

            send_buffer += '|' + std::to_string(MSG_get_clock()) + ":C:" + std::to_string(job->id);
            XBT_INFO("Message to send to scheduler: %s", send_buffer.c_str());

            break;
        } // end of case JOB_COMPLETED
        case IPMessageType::JOB_SUBMITTED:
        {
            xbt_assert(task_data->data != nullptr);
            JobSubmittedMessage * message = (JobSubmittedMessage *) task_data->data;

            nb_submitted_jobs++;
            Job * job = context->jobs[message->job_id];
            job->state = JobState::JOB_STATE_SUBMITTED;

            XBT_INFO("Job %d SUBMITTED. %d jobs submitted so far", job->id, nb_submitted_jobs);
            send_buffer += "|" + std::to_string(MSG_get_clock()) + ":S:" + std::to_string(job->id);
            XBT_INFO("Message to send to scheduler: %s", send_buffer.c_str());

            break;
        } // end of case JOB_SUBMITTED
        case IPMessageType::SCHED_NOP:
        {
            XBT_INFO("Nothing to do received.");
            if (nb_running_jobs == 0 && nb_scheduled_jobs < nb_submitted_jobs)
            {
                XBT_INFO("Nothing to do whereas no job is running and that they are jobs waiting to be scheduled... This might cause a deadlock!");

                // Let us display the available jobs (to help the scheduler debugging)
                string debugBuffer;

                const std::map<int, Job *> & jobs = context->jobs.jobs();
                vector<string> submittedJobs;

                for (auto & mit : jobs)
                {
                    if (mit.second->state == JobState::JOB_STATE_SUBMITTED)
                        submittedJobs.push_back(std::to_string(mit.second->id));
                }

                string submittedJobsString = boost::algorithm::join(submittedJobs, ", ");

                XBT_INFO("The available jobs are [%s]", submittedJobsString.c_str());
            }
            break;
        }
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

                ExecuteJobProcessArguments * exec_args = new ExecuteJobProcessArguments;
                exec_args->context = context;
                exec_args->allocation = allocation;
                string pname = "job" + to_string(job->id);
                MSG_process_create(pname.c_str(), execute_job_process, (void*)exec_args, context->machines[allocation.machine_ids[0]]->host);
            }

            break;
        }
        case IPMessageType::SCHED_READY:
        {
            sched_ready = true;
            break;
        }
        } // end of switch

        delete task_data;
        MSG_task_destroy(task_received);

        if (sched_ready && !send_buffer.empty())
        {
            RequestReplyProcessArguments * req_rep_args = new RequestReplyProcessArguments;
            req_rep_args->context = context;
            req_rep_args->send_buffer = "0:" + to_string(MSG_get_clock()) + send_buffer;
            send_buffer.clear();

            MSG_process_create("Scheduler REQ-REP", request_reply_scheduler_process, (void*)req_rep_args, MSG_host_self());
            sched_ready = false;
        }

    } // end of while

    XBT_INFO("All jobs completed!");

    delete args;
    return 0;
}

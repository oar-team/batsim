/**
 * @file ipp.cpp
 * @brief Inter-Process Protocol (within Batsim, not with the Decision real process)
 */

#include "ipp.hpp"

#include <simgrid/msg.h>

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(ipp, "ipp"); //!< Logging

/**
 * @brief TODO
 * @param destination_mailbox TODO
 * @param type TODO
 * @param data TODO
 * @param detached TODO
 */
void generic_send_message(const std::string & destination_mailbox, IPMessageType type, void * data, bool detached)
{
    IPMessage * message = new IPMessage;
    message->type = type;
    message->data = data;

    msg_task_t task_to_send = MSG_task_create(NULL, 0, 1e-6, message);

    XBT_INFO("message from '%s' to '%s' of type '%s' with data %p",
             MSG_process_get_name(MSG_process_self()), destination_mailbox.c_str(),
             ipMessageTypeToString(type).c_str(), data);

    if (detached)
    {
        MSG_task_dsend(task_to_send, destination_mailbox.c_str(), NULL);
    }
    else
    {
        MSG_task_send(task_to_send, destination_mailbox.c_str());
    }

    XBT_INFO("message from '%s' to '%s' of type '%s' with data %p done",
             MSG_process_get_name(MSG_process_self()), destination_mailbox.c_str(),
             ipMessageTypeToString(type).c_str(), data);
}

void send_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
  generic_send_message(destination_mailbox, type, data, false);
}

void dsend_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
  generic_send_message(destination_mailbox, type, data, true);
}

std::string ipMessageTypeToString(IPMessageType type)
{
    string s;

    switch(type)
    {
        case IPMessageType::JOB_SUBMITTED:
            s = "JOB_SUBMITTED";
            break;
        case IPMessageType::JOB_COMPLETED:
            s = "JOB_COMPLETED";
            break;
        case IPMessageType::PSTATE_MODIFICATION:
            s = "PSTATE_MODIFICATION";
            break;
        case IPMessageType::SCHED_ALLOCATION:
            s = "SCHED_ALLOCATION";
            break;
        case IPMessageType::SCHED_REJECTION:
            s = "SCHED_REJECTION";
            break;
        case IPMessageType::SCHED_NOP:
            s = "SCHED_NOP";
            break;
        case IPMessageType::SCHED_NOP_ME_LATER:
            s = "SCHED_NOP_ME_LATER";
            break;
        case IPMessageType::SCHED_TELL_ME_ENERGY:
            s = "SCHED_TELL_ME_ENERGY";
            break;
        case IPMessageType::SCHED_READY:
            s = "SCHED_READY";
            break;
        case IPMessageType::WAITING_DONE:
            s = "WAITING_DONE";
            break;
        case IPMessageType::SUBMITTER_HELLO:
            s = "SUBMITTER_HELLO";
            break;
        case IPMessageType::SUBMITTER_CALLBACK:
            s = "SUBMITTER_CALLBACK";
            break;
        case IPMessageType::SUBMITTER_BYE:
            s = "SUBMITTER_BYE";
            break;
        case IPMessageType::SWITCHED_ON:
            s = "SWITCHED_ON";
            break;
        case IPMessageType::SWITCHED_OFF:
            s = "SWITCHED_OFF";
            break;
    }

    return s;
}

void send_message(const char *destination_mailbox, IPMessageType type, void *data)
{
    const string str = destination_mailbox;
    send_message(str, type, data);
}

void dsend_message(const char *destination_mailbox, IPMessageType type, void *data)
{
    const string str = destination_mailbox;
    dsend_message(str, type, data);
}

IPMessage::~IPMessage()
{
    switch (type)
    {
        case IPMessageType::JOB_SUBMITTED:
        {
            JobSubmittedMessage * msg = (JobSubmittedMessage *) data;
            delete msg;
        } break;
        case IPMessageType::JOB_COMPLETED:
        {
            JobCompletedMessage * msg = (JobCompletedMessage *) data;
            delete msg;
        } break;
        case IPMessageType::PSTATE_MODIFICATION:
        {
            PStateModificationMessage * msg = (PStateModificationMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SCHED_ALLOCATION:
        {
            SchedulingAllocationMessage * msg = (SchedulingAllocationMessage *) data;
            // The Allocations themselves are not memory-deallocated there but at the end of the job execution.
            delete msg;
        } break;
        case IPMessageType::SCHED_REJECTION:
        {
            JobRejectedMessage * msg = (JobRejectedMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SCHED_NOP_ME_LATER:
        {
            NOPMeLaterMessage * msg = (NOPMeLaterMessage*) data;
            delete msg;
        } break;
        case IPMessageType::SCHED_NOP:
        {
        } break;
        case IPMessageType::SCHED_TELL_ME_ENERGY:
        {
        } break;
        case IPMessageType::SCHED_READY:
        {
        } break;
        case IPMessageType::SUBMITTER_HELLO:
        {
            SubmitterHelloMessage * msg = (SubmitterHelloMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SUBMITTER_CALLBACK:
        {
            SubmitterJobCompletionCallbackMessage * msg = (SubmitterJobCompletionCallbackMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SUBMITTER_BYE:
        {
            SubmitterByeMessage * msg = (SubmitterByeMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SWITCHED_ON:
        {
            SwitchONMessage * msg = (SwitchONMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SWITCHED_OFF:
        {
            SwitchOFFMessage * msg = (SwitchOFFMessage *) data;
            delete msg;
        } break;
        case IPMessageType::WAITING_DONE:
        {
        } break;
    }

    data = nullptr;
}

JobIdentifier::JobIdentifier(const string &workload_name, int job_number) :
    workload_name(workload_name),
    job_number(job_number)
{

}

string JobIdentifier::to_string() const
{
    return workload_name + '!' + std::to_string(job_number);
}

bool operator<(const JobIdentifier &ji1, const JobIdentifier &ji2)
{
    return ji1.to_string() < ji2.to_string();
}

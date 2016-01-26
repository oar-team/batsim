#include "ipp.hpp"

#include <simgrid/msg.h>

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(ipp, "ipp");

void send_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
    IPMessage * message = new IPMessage;
    message->type = type;
    message->data = data;

    msg_task_t task_to_send = MSG_task_create(NULL, 0, 1e-6, message);

    XBT_INFO("message from '%s' to '%s' of type '%s' with data %p",
             MSG_process_get_name(MSG_process_self()), destination_mailbox.c_str(), ipMessageTypeToString(type).c_str(), data);

    MSG_task_send(task_to_send, destination_mailbox.c_str());
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
        } break;
        case IPMessageType::SUBMITTER_BYE:
        {
        } break;
        case IPMessageType::SWITCHED_ON:
        {
            PStateModificationMessage * msg = (PStateModificationMessage *) data;
            delete msg;
        } break;
        case IPMessageType::SWITCHED_OFF:
        {
            PStateModificationMessage * msg = (PStateModificationMessage *) data;
            delete msg;
        } break;
        case IPMessageType::WAITING_DONE:
        {
        } break;
    }

    data = nullptr;
}

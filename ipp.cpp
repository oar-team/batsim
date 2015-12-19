#include "ipp.hpp"

#include <simgrid/msg.h>
#include <map>

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
    static map<IPMessageType, string> type_to_string =
    {
        {IPMessageType::JOB_SUBMITTED, "JOB_SUBMITTED"},
        {IPMessageType::JOB_COMPLETED, "JOB_COMPLETED"},
        {IPMessageType::SCHED_ALLOCATION, "SCHED_ALLOCATION"},
        {IPMessageType::SCHED_NOP, "SCHED_NOP"},
        {IPMessageType::SCHED_READY, "SCHED_READY"},
        {IPMessageType::SUBMITTER_HELLO, "SUBMITTER_HELLO"},
        {IPMessageType::SUBMITTER_BYE, "SUBMITTER_BYE"},
    };

    return type_to_string[type];
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

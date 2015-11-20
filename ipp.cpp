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

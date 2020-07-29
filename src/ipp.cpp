/**
 * @file ipp.cpp
 * @brief Inter-Process Protocol (within Batsim, not with the Decision real process)
 */

#include "ipp.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(ipp, "ipp"); //!< Logging

void generic_send_message(const std::string & destination_mailbox,
                          IPMessageType type,
                          void * data,
                          bool detached)
{
    IPMessage * message = new IPMessage;
    message->type = type;
    message->data = data;

    auto mailbox = simgrid::s4u::Mailbox::by_name(destination_mailbox);
    const uint64_t message_size = 1;

    XBT_DEBUG("message from '%s' to '%s' of type '%s' with data %p",
              simgrid::s4u::this_actor::get_cname(), destination_mailbox.c_str(),
              ip_message_type_to_string(type).c_str(), data);

    if (detached)
    {
        mailbox->put_async(message, message_size);
    }
    else
    {
        mailbox->put(message, message_size);
    }

    XBT_DEBUG("message from '%s' to '%s' of type '%s' with data %p done",
              simgrid::s4u::this_actor::get_cname(), destination_mailbox.c_str(),
              ip_message_type_to_string(type).c_str(), data);
}

void send_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
    generic_send_message(destination_mailbox, type, data, false);
}

void dsend_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
    generic_send_message(destination_mailbox, type, data, true);
}

IPMessage * receive_message(const std::string & reception_mailbox)
{
    auto mailbox = simgrid::s4u::Mailbox::by_name(reception_mailbox);
    IPMessage * message = static_cast<IPMessage *>(mailbox->get());
    return message;
}

bool mailbox_empty(const std::string & reception_mailbox)
{
    auto mailbox = simgrid::s4u::Mailbox::by_name(reception_mailbox);
    return mailbox->empty();
}

std::string ip_message_type_to_string(IPMessageType type)
{
    string s;

    // Do not remove the switch. If one adds a new IPMessageType but forgets to handle it in the
    // switch, a compilation warning should help avoiding this bug.
    switch(type)
    {
        case IPMessageType::JOB_SUBMITTED:
            s = "JOB_SUBMITTED";
            break;
        case IPMessageType::JOB_REGISTERED_BY_DP:
            s = "JOB_REGISTERED_BY_DP";
            break;
        case IPMessageType::PROFILE_REGISTERED_BY_DP:
            s = "PROFILE_REGISTERED_BY_DP";
            break;
        case IPMessageType::JOB_COMPLETED:
            s = "JOB_COMPLETED";
            break;
        case IPMessageType::PSTATE_MODIFICATION:
            s = "PSTATE_MODIFICATION";
            break;
        case IPMessageType::SCHED_EXECUTE_JOB:
            s = "SCHED_EXECUTE_JOB";
            break;
        case IPMessageType::SCHED_CHANGE_JOB_STATE:
            s = "SCHED_CHANGE_JOB_STATE";
            break;
        case IPMessageType::SCHED_REJECT_JOB:
            s = "SCHED_REJECT_JOB";
            break;
        case IPMessageType::SCHED_KILL_JOB:
            s = "SCHED_KILL_JOB";
            break;
        case IPMessageType::SCHED_CALL_ME_LATER:
            s = "SCHED_CALL_ME_LATER";
            break;
        case IPMessageType::SCHED_TELL_ME_ENERGY:
            s = "SCHED_TELL_ME_ENERGY";
            break;
        case IPMessageType::SCHED_WAIT_ANSWER:
            s = "SCHED_WAIT_ANSWER";
            break;
        case IPMessageType::SCHED_SET_JOB_METADATA:
            s = "SCHED_SET_JOB_METADATA";
            break;
        case IPMessageType::WAIT_QUERY:
            s = "WAIT_QUERY";
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
        case IPMessageType::KILLING_DONE:
            s = "KILLING_DONE";
            break;
        case IPMessageType::END_DYNAMIC_REGISTER:
            s = "END_DYNAMIC_REGISTER";
            break;
        case IPMessageType::CONTINUE_DYNAMIC_REGISTER:
            s = "CONTINUE_DYNAMIC_REGISTER";
            break;
        case IPMessageType::TO_JOB_MSG:
            s = "TO_JOB_MSG";
            break;
        case IPMessageType::FROM_JOB_MSG:
            s = "FROM_JOB_MSG";
            break;
        case IPMessageType::EVENT_OCCURRED:
            s = "EVENT_OCCURRED";
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
    // Do not remove the switch. If one adds a new IPMessageType but forgets to handle it in the
    // switch, a compilation warning should help avoiding this bug.
    switch (type)
    {
        case IPMessageType::JOB_SUBMITTED:
        {
            auto * msg = static_cast<JobSubmittedMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::JOB_REGISTERED_BY_DP:
        {
            auto * msg = static_cast<JobRegisteredByDPMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::PROFILE_REGISTERED_BY_DP:
        {
            auto * msg = static_cast<ProfileRegisteredByDPMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::JOB_COMPLETED:
        {
            auto * msg = static_cast<JobCompletedMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::PSTATE_MODIFICATION:
        {
            auto * msg = static_cast<PStateModificationMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_EXECUTE_JOB:
        {
            auto * msg = static_cast<ExecuteJobMessage *>(data);
            // The Allocations themselves are not memory-deallocated there but at the end of the job execution.
            delete msg;
        } break;
        case IPMessageType::SCHED_CHANGE_JOB_STATE:
        {
            auto * msg = static_cast<ChangeJobStateMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_REJECT_JOB:
        {
            auto * msg = static_cast<JobRejectedMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_KILL_JOB:
        {
            auto * msg = static_cast<KillJobMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_CALL_ME_LATER:
        {
            auto * msg = static_cast<CallMeLaterMessage*>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_TELL_ME_ENERGY:
        {
        } break;
        case IPMessageType::SCHED_SET_JOB_METADATA:
        {
            auto * msg = static_cast<SetJobMetadataMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::WAIT_QUERY:
        {
            auto * msg = static_cast<WaitQueryMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_WAIT_ANSWER:
        {
            auto * msg = static_cast<SchedWaitAnswerMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_READY:
        {
        } break;
        case IPMessageType::SUBMITTER_HELLO:
        {
            auto * msg = static_cast<SubmitterHelloMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SUBMITTER_CALLBACK:
        {
            auto * msg = static_cast<SubmitterJobCompletionCallbackMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SUBMITTER_BYE:
        {
            auto * msg = static_cast<SubmitterByeMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SWITCHED_ON:
        {
            auto * msg = static_cast<SwitchMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SWITCHED_OFF:
        {
            auto * msg = static_cast<SwitchMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::WAITING_DONE:
        {
        } break;
        case IPMessageType::KILLING_DONE:
        {
            auto * msg = static_cast<KillingDoneMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::END_DYNAMIC_REGISTER:
        {
        } break;
        case IPMessageType::CONTINUE_DYNAMIC_REGISTER:
        {
        } break;
        case IPMessageType::TO_JOB_MSG:
        {
            auto * msg = static_cast<ToJobMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::FROM_JOB_MSG:
        {
            auto * msg = static_cast<FromJobMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::EVENT_OCCURRED:
        {
            auto * msg = static_cast<EventOccurredMessage *>(data);
            delete msg;
        } break;
    }

    data = nullptr;
}

std::string submitter_type_to_string(SubmitterType type)
{
    string s;
    // Do not remove the switch. If one adds a new SubmitterType but forgets to handle it in
    // the switch, a compilation warning should help avoiding this bug.
    switch(type)
    {
        case SubmitterType::EVENT_SUBMITTER:
            s = "Event";
            break;
        case SubmitterType::JOB_SUBMITTER:
            s = "Job";
            break;
    }
    return s;
}

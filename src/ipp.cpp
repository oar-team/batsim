/**
 * @file ipp.cpp
 * @brief Inter-Process Protocol (within Batsim, not with the Decision real process)
 */

#include "ipp.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(ipp, "ipp"); //!< Logging

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of the message to send
 * @param[in] data The data associated with the message
 * @param[in] detached Whether the send should be detached (put or put_async)
 */
void generic_send_message(
    const std::string & destination_mailbox,
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
        auto comm = mailbox->put_async(message, message_size);
        comm.detach();
    }
    else
    {
        mailbox->put(message, message_size);
    }

    XBT_DEBUG("message from '%s' to '%s' of type '%s' with data %p done",
              simgrid::s4u::this_actor::get_cname(), destination_mailbox.c_str(),
              ip_message_type_to_string(type).c_str(), data);
}

/**
 * @brief Send an inter-actor message at a given time, sleeping until the desired time is reached if needed
 * @param[in] destination_mailbox The mailbox onto which the message should be put
 * @param[in] message The message to send
 * @param[in] when The time at which the message should be sent
 * @param[in] detached If false, this function returns when the message has been transferred (time increases after "when"). Otherwise, this function returns quickly and time does not increase beyond "when".
 */
void send_message_at_time(
    const std::string & destination_mailbox,
    IPMessage * message,
    double when,
    bool detached)
{
    // Wait until "when" time point is reached, if needed
    double current_time = simgrid::s4u::Engine::get_clock();
    if (when > current_time)
    {
        simgrid::s4u::this_actor::sleep_for(when - current_time);
    }

    // Actually send the message
    auto mailbox = simgrid::s4u::Mailbox::by_name(destination_mailbox);
    const uint64_t message_size = 1;

    if (detached)
    {
        mailbox->put_async(message, message_size);
    }
    else
    {
        mailbox->put(message, message_size);
    }
}

/**
 * @brief Send an inter-actor message at a given time, sleeping until the desired time is reached if needed
 * @param[in] destination_mailbox The mailbox onto which the message should be put
 * @param[in] type The inter-actor message type to send
 * @param[in] data The inter-actor message data to send
 * @param[in] when The time at which the message should be sent
 * @param[in] detached If false, this function returns when the message has been transferred (time increases after "when"). Otherwise, this function returns quickly and time does not increase beyond "when".
 */
void send_message_at_time(
    const std::string & destination_mailbox,
    IPMessageType type,
    void * data,
    double when,
    bool detached)
{
    // Wait until "when" time point is reached, if needed
    double current_time = simgrid::s4u::Engine::get_clock();
    if (when > current_time)
    {
        simgrid::s4u::this_actor::sleep_for(when - current_time);
    }

    // Actually send the message
    generic_send_message(destination_mailbox, type, data, detached);
}

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void send_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
    generic_send_message(destination_mailbox, type, data, false);
}

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void send_message(const char *destination_mailbox, IPMessageType type, void *data)
{
    const string str = destination_mailbox;
    send_message(str, type, data);
}

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void dsend_message(const std::string & destination_mailbox, IPMessageType type, void * data)
{
    generic_send_message(destination_mailbox, type, data, true);
}

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void dsend_message(const char *destination_mailbox, IPMessageType type, void *data)
{
    const string str = destination_mailbox;
    dsend_message(str, type, data);
}

/**
 * @brief Receive a message on a given mailbox
 * @param[in] reception_mailbox The mailbox name
 * @return The received message. Must be deallocated by the caller.
 */
IPMessage * receive_message(const std::string & reception_mailbox)
{
    auto mailbox = simgrid::s4u::Mailbox::by_name(reception_mailbox);
    IPMessage * message = mailbox->get<IPMessage>();
    return message;
}



/**
 * @brief Clear the mailbox
 * @param[in] reception_mailbox The mailbox name
 */
void clear_mailbox(const std::string & reception_mailbox)
{
    auto mailbox = simgrid::s4u::Mailbox::by_name(reception_mailbox);
    mailbox->clear();
}

/**
 * @brief Check if the mailbox is empty
 * @param[in] reception_mailbox The mailbox name
 * @return Boolean indicating if the mailbox is empty
 */
bool mailbox_empty(const std::string & reception_mailbox)
{
    auto mailbox = simgrid::s4u::Mailbox::by_name(reception_mailbox);
    return mailbox->empty();
}

/**
 * @brief Transforms a IPMessageType into a std::string
 * @param[in] type The IPMessageType
 * @return The std::string corresponding to the type
 */
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
        case IPMessageType::SCHED_JOB_REGISTERED:
            s = "SCHED_JOB_REGISTERED";
            break;
        case IPMessageType::SCHED_PROFILE_REGISTERED:
            s = "SCHED_PROFILE_REGISTERED";
            break;
        case IPMessageType::JOB_COMPLETED:
            s = "JOB_COMPLETED";
            break;
        case IPMessageType::SCHED_CHANGE_HOST_PSTATE:
            s = "SCHED_CHANGE_HOST_PSTATE";
            break;
        case IPMessageType::SCHED_EXECUTE_JOB:
            s = "SCHED_EXECUTE_JOB";
            break;
        case IPMessageType::SCHED_HELLO:
            s = "SCHED_HELLO";
            break;
        case IPMessageType::SCHED_REJECT_JOB:
            s = "SCHED_REJECT_JOB";
            break;
        case IPMessageType::SCHED_KILL_JOBS:
            s = "SCHED_KILL_JOB";
            break;
        case IPMessageType::SCHED_CALL_ME_LATER:
            s = "SCHED_CALL_ME_LATER";
            break;
        case IPMessageType::SCHED_STOP_CALL_ME_LATER:
            s = "SCHED_STOP_CALL_ME_LATER";
            break;
        case IPMessageType::SCHED_CREATE_PROBE:
            s = "SCHED_CREATE_PROBE";
            break;
        case IPMessageType::SCHED_STOP_PROBE:
            s = "SCHED_STOP_PROBE";
            break;
        case IPMessageType::SCHED_READY:
            s = "SCHED_READY";
            break;
        case IPMessageType::ONESHOT_REQUESTED_CALL:
            s = "ONESHOT_REQUESTED_CALL";
            break;
        case IPMessageType::PERIODIC_TRIGGER:
            s = "PERIODIC_TRIGGER";
            break;
        case IPMessageType::PERIODIC_ENTITY_STOPPED:
            s = "PERIODIC_ENTITY_STOPPED";
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
        case IPMessageType::SCHED_END_DYNAMIC_REGISTRATION:
            s = "SCHED_END_DYNAMIC_REGISTRATION";
            break;
        case IPMessageType::SCHED_FORCE_SIMULATION_STOP:
            s = "SCHED_FORCE_SIMULATION_STOP";
            break;
        case IPMessageType::EXTERNAL_EVENTS_OCCURRED:
            s = "EXTERNAL_EVENTS_OCCURRED";
            break;
        case IPMessageType::DIE:
            s = "DIE";
            break;
    }

    return s;
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
        case IPMessageType::SCHED_JOB_REGISTERED:
        {
            auto * msg = static_cast<JobRegisteredByEDCMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_PROFILE_REGISTERED:
        {
            auto * msg = static_cast<ProfileRegisteredByEDCMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::JOB_COMPLETED:
        {
            auto * msg = static_cast<JobCompletedMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_CHANGE_HOST_PSTATE:
        {
            auto * msg = static_cast<ChangeHostPStateMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_EXECUTE_JOB:
        {
            // As the data contained in it is needed during the job execution,
            // this data lifetime is bound to the job's lifetime.
            // In other words, data deallocation will be done when the corresponding job will be deallocated.
        } break;
        case IPMessageType::SCHED_HELLO:
        {
            auto * msg = static_cast<EDCHelloMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_REJECT_JOB:
        {
            auto * msg = static_cast<RejectJobMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_KILL_JOBS:
        {
            // The data of this event is used during the full lifetime of the jobs kill,
            // it is deallocated when the kills have been done.
        } break;
        case IPMessageType::SCHED_CALL_ME_LATER:
        {
            // The lifetime of these messages depend on their periodicity.
            // - non-periodic messages are deleted explictly by the server actor.
            // - periodic messages are forwarded to the periodic actor, which is then responsible to deallocate them.
        } break;
        case IPMessageType::SCHED_STOP_CALL_ME_LATER:
        {
            // Forwarded to the periodic actor, which is then responsible to deallocate them
        } break;
        case IPMessageType::SCHED_CREATE_PROBE:
        {
            // Forwarded to the periodic actor, which is then responsible to deallocate them
        } break;
        case IPMessageType::SCHED_STOP_PROBE:
        {
            // Forwarded to the periodic actor, which is then responsible to deallocate them
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
        case IPMessageType::ONESHOT_REQUESTED_CALL:
        {
            auto * msg = static_cast<OneShotRequestedCallMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::PERIODIC_TRIGGER:
        {
            auto * msg = static_cast<PeriodicTriggerMessage *>(data);
            for (auto * probe_data : msg->probes_data)
                delete probe_data;
            delete msg;
        } break;
        case IPMessageType::PERIODIC_ENTITY_STOPPED:
        {
            auto * msg = static_cast<PeriodicEntityStoppedMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::KILLING_DONE:
        {
            auto * msg = static_cast<KillingDoneMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::SCHED_END_DYNAMIC_REGISTRATION:
        {
            // No data in this event
        } break;
        case IPMessageType::SCHED_FORCE_SIMULATION_STOP:
        {
            // No data in this event
        } break;
        case IPMessageType::EXTERNAL_EVENTS_OCCURRED:
        {
            auto * msg = static_cast<ExternalEventsOccurredMessage *>(data);
            delete msg;
        } break;
        case IPMessageType::DIE:
        {
        } break;
    }

    data = nullptr;
}

KillingDoneMessage::~KillingDoneMessage()
{
    delete kill_jobs_message;
    kill_jobs_message = nullptr;
}

/**
 * @brief Transforms a SumbitterType into a std::string
 * @param[in] type The SubmitterType
 * @return The std::string corresponding to the type
 */
std::string submitter_type_to_string(SubmitterType type)
{
    string s;
    // Do not remove the switch. If one adds a new SubmitterType but forgets to handle it in
    // the switch, a compilation warning should help avoiding this bug.
    switch(type)
    {
        case SubmitterType::EXTERNAL_EVENT_SUBMITTER:
            s = "Event";
            break;
        case SubmitterType::JOB_SUBMITTER:
            s = "Job";
            break;
    }
    return s;
}

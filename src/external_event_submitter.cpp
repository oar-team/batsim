/*
 * @file external_event_submitter.cpp
 * @brief Contains functions related to event submission
 */

#include "external_event_submitter.hpp"

#include <vector>
#include <algorithm>

#include <simgrid/s4u.hpp>

#include "ipp.hpp"
#include "context.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(external_event_submitter, "external_event_submitter");

using namespace std;

static void send_events_to_server(const vector<const ExternalEvent *> & events_to_send,
                                  const string & submitter_name)
{
    if (!events_to_send.empty())
    {
        ExternalEventsOccurredMessage * msg = new ExternalEventsOccurredMessage;
        msg->submitter_name = submitter_name;
        msg->occurred_events = events_to_send;
        send_message("server", IPMessageType::EXTERNAL_EVENTS_OCCURRED, static_cast<void*>(msg));
    }
}

void static_external_event_submitter_process(BatsimContext * context,
                                    std::string eventList_name)
{
    xbt_assert(context->external_event_lists.count(eventList_name) == 1,
               "Error: a static_external_event_submitter_process is in charge of the event list '%s' "
               "which does not exist.", eventList_name.c_str());

    string submitter_name = eventList_name + "_submitter";
    /*
        ███████████████████████████████─
        ──────▀█▄▀▄▀████▀──▀█▄▀▄▀████▀──
        ────────▀█▄█▄█▀──────▀█▄█▄█▀────
    */

    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = false;
    hello_msg->submitter_type = SubmitterType::EXTERNAL_EVENT_SUBMITTER;

    send_message("server", IPMessageType::SUBMITTER_HELLO, static_cast<void*>(hello_msg));

    long double current_occurring_date = static_cast<long double>(simgrid::s4u::Engine::get_clock());

    const vector<ExternalEvent *> & eventVector = context->external_event_lists[eventList_name]->events();

    if(eventVector.size() > 0)
    {
        vector<const ExternalEvent *> events_to_send;

        for(const ExternalEvent * e : eventVector)
        {
            if (e->timestamp > current_occurring_date)
            {
                // Next event occurs after current time, send the message to the server for previous occured events
                send_events_to_server(events_to_send, submitter_name);
                events_to_send.clear();

                // Now let's sleep until it's time for the next event to occur
                simgrid::s4u::this_actor::sleep_for(static_cast<double>(e->timestamp - current_occurring_date));
                current_occurring_date = static_cast<long double>(simgrid::s4u::Engine::get_clock());
            }

            // Populate the vector of events to send to the server
            events_to_send.push_back(e);
        }

        // Send last vector of events to occur
        send_events_to_server(events_to_send, submitter_name);
    }

    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->is_workflow_submitter = false;
    bye_msg->submitter_type = SubmitterType::EXTERNAL_EVENT_SUBMITTER;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, static_cast<void*>(bye_msg));
}

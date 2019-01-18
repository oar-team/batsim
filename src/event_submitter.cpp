/*
 * @file event_submitter.cpp
 * @brief Contains functions related to event submission
 */

#include "event_submitter.hpp"

#include <vector>
#include <algorithm>

#include <simgrid/s4u.hpp>

#include "ipp.hpp"
#include "context.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(event_submitter, "event_submitter");

using namespace std;

static void send_events_to_server(const vector<const Event *> & events_to_send,
                                  const string & submitter_name)
{
    if (!events_to_send.empty())
    {
        EventOccurredMessage * msg = new EventOccurredMessage;
        msg->submitter_name = submitter_name;
        msg->occurred_events = events_to_send;
        send_message("server", IPMessageType::EVENT_OCCURRED, (void*)msg);
    }
}

void static_event_submitter_process(BatsimContext * context,
                                    std::string events_name)
{
    xbt_assert(context->eventsMap.count(events_name) == 1,
               "Error: a static_event_submitter_process is in charge of the event list '%' "
               "which does not exist.", events_name.c_str());

    Events * events = context->eventsMap[events_name];
    string submitter_name = events_name + "_submitter";
    /*
        ███████████████████████████████─
        ──────▀█▄▀▄▀████▀──▀█▄▀▄▀████▀──
        ────────▀█▄█▄█▀──────▀█▄█▄█▀────
    */

    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = false;

    send_message("server", IPMessageType::SUBMITTER_HELLO, (void*) hello_msg);

    long double current_occurring_date = simgrid::s4u::Engine::get_clock();

    const vector<Event *> & eventVector = events->getEvents();

    if(eventVector.size() > 0)
    {
        vector<const Event *> events_to_send;

        for(const Event * e : eventVector)
        {
            if (e->timestamp > current_occurring_date)
            {
                // Next event occurs after current time, send the message to the server for previous occured events
                send_events_to_server(events_to_send, submitter_name);
                events_to_send.clear();

                // Now let's sleep until it's time for the next event to occur
                simgrid::s4u::this_actor::sleep_for((double)e->timestamp - (double) current_occurring_date);
                current_occurring_date = simgrid::s4u::Engine::get_clock();
            }

            // Populate the vector of events to send to the server
            events_to_send.push_back(e);
        }

        // Send last vector of events to occur
        send_events_to_server(events_to_send, submitter_name);
    }

    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->is_workflow_submitter = false;
    bye_msg->is_event_submitter = true;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, (void *) bye_msg);
}

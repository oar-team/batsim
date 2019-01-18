/**
 * @file event_submitter.hpp
 * @brief Contains functions related to event submission
 */

#pragma once

#include <string>

struct BatsimContext;

/**
 * @brief The process in charge if submitting static events
 * @param context The BatsimContext
 * @param events_name The name of the event list attached to the submitter
 */
void static_event_submitter_process(BatsimContext * context,
                                    std::string events_name);

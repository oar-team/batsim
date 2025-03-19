/**
 * @file external_event_submitter.hpp
 * @brief Contains functions related to event submission
 */

#pragma once

#include <string>

struct BatsimContext;

/**
 * @brief The process in charge if submitting static events
 * @param context The BatsimContext
 * @param eventList_name The name of the external event list attached to the submitter
 */
void static_external_event_submitter_process(BatsimContext * context,
                                             std::string eventList_name);

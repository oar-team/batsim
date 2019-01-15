/**
 * @file network.hpp
 * @brief Contains network-related classes and functions
 */

#pragma once

#include <string>

#include "ipp.hpp"
struct BatsimContext;

/**
 * @brief The process in charge of doing a Request-Reply iteration with the Decision real process
 * @details This process sends a message to the Decision real process (Request) then waits for the answered message (Reply)
 * @param[in] context The BatsimContext
 * @param[in] send_buffer The message to send to the Decision real process
 */
void request_reply_scheduler_process(BatsimContext *context, std::string send_buffer);

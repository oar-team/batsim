/**
 * @file network.hpp
 * @brief Contains network-related classes and functions
 */

#pragma once

#include <string>

#include "ipp.hpp"

struct BatsimContext;

void request_reply_scheduler_process(BatsimContext *context, std::string send_buffer);

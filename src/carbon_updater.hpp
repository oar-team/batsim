/**
 * @file carbon_updater.hpp
 * @brief Contains functions related to carbon footprint updates
 */

#pragma once

#include <string>
#include <map>
#include <unordered_map>

/**
 * @brief The process in charge of updating the carbon footprint of a host
 * @param host_id The id of the host to update 
 * @param host_carbon_intensities The map of carbon rates for the host
 */
void carbon_updater_actor_for_host(const std::string& host_id,
    const std::map<std::string, double>& host_carbon_intensities);

std::unordered_map<std::string, std::map<std::string, double>> load_carbon_trace_file(const std::string &trace_file);

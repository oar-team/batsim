/*
 * @file carbon_updater.cpp
 * @brief Contains functions related to carbon footprint updates
 */

#include "carbon_updater.hpp"
#include <simgrid/plugins/carbon_footprint.h>
#include <simgrid/s4u.hpp>
#include <fstream>
#include <xbt/log.h>
#include <algorithm>
#include <sstream>

XBT_LOG_NEW_DEFAULT_CATEGORY(carbon_updater, "carbon_updater");

using namespace std;

void carbon_updater_actor_for_host(const std::string& host_id, const std::map<std::string, double> & host_carbon_intensities) 
{

    simgrid::s4u::Host* host = simgrid::s4u::Host::by_name(host_id);
    if (!host) {
        XBT_ERROR("Host '%s' was not found on the platform. Shutting down actor.", host_id.c_str());
        return;
    }

    double current_time = simgrid::s4u::Engine::get_clock();
    
    for (auto it = host_carbon_intensities.begin(); it != host_carbon_intensities.end(); ++it) {
        double event_time = std::stod(it->first);
        double carbon_intensity = it->second;     
    
        double sleep_time = event_time - current_time;
    
        if (sleep_time > 0) {
            XBT_INFO("Sleeping for %.2f seconds until the next event for host '%s'.", sleep_time, host_id.c_str());
            simgrid::s4u::this_actor::sleep_for(sleep_time);
        }
    
        current_time = simgrid::s4u::Engine::get_clock();
        sg_host_set_carbon_intensity(host, carbon_intensity);
    
        XBT_DEBUG("Updating carbon intensity of host '%s' to %.2f at timestamp %.2f.",
                    host_id.c_str(), carbon_intensity, event_time);

        }
}

std::unordered_map<std::string, std::map<std::string, double>> load_carbon_trace_file(const std::string &trace_file) {
    std::unordered_map<std::string, std::map<std::string, double>> carbon_intensities;

    std::ifstream file(trace_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open carbon trace file: " + trace_file);
    }

    std::string line;
    std::getline(file, line); 

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string host_id, timestamp, carbon_intensity_str;

        try {
            if (std::getline(ss, host_id, ',') &&
                std::getline(ss, timestamp, ',') &&
                std::getline(ss, carbon_intensity_str)) {
                double carbon_intensity = std::stod(carbon_intensity_str); 
                carbon_intensities[host_id][timestamp] = carbon_intensity;
            } else {
                XBT_WARN("Malformed line in carbon trace file: '%s'. Skipping.", line.c_str());
            }
        } catch (const std::exception &e) {
            XBT_WARN("Error parsing line in carbon trace file: '%s'. Skipping. Error: %s", line.c_str(), e.what());
        }
    }

    file.close();
    return carbon_intensities;
}

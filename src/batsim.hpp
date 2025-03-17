#pragma once

#include <string>
#include <list>
#include <map>
#include <vector>

#include <rapidjson/document.h>

#include "cli.hpp"

struct BatsimContext;

/**
 * @brief Configures how the simulation should be logged
 * @param[in,out] main_args Batsim arguments
 */
void configure_batsim_logging_output(const MainArguments & main_args);

/**
 * @brief Loads the workloads defined in Batsim arguments
 * @param[in] main_args Batsim arguments
 * @param[in,out] context The BatsimContext
 * @param[out] max_nb_machines_to_use The maximum number of machines that should be used in the simulation.
 *             This number is computed from Batsim arguments but depends on Workloads content. -1 means no limitation.
 */
void load_workloads_and_workflows(const MainArguments & main_args, BatsimContext * context, int & max_nb_machines_to_use);

/**
 * @brief Loads the eventLists defined in Batsim arguments
 * @param[in] main_args Batsim arguments
 * @param[in,out] context The BatsimContext
 */
void load_eventLists(const MainArguments & main_args, BatsimContext * context);

/**
 * @brief Starts the SimGrid processes that should be executed at the beginning of the simulation
 * @param[in] main_args Batsim arguments
 * @param[in] context The BatsimContext
 */
void start_initial_simulation_processes(const MainArguments & main_args,
                                        BatsimContext * context);

/**
 * @brief Sets the simulation configuration
 * @param[in,out] context The BatsimContext
 * @param[in] main_args Batsim arguments
 */
void set_configuration(BatsimContext * context,
                       MainArguments & main_args);

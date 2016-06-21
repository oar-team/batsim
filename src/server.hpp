/**
 * @file server.hpp
 * @brief Contains functions related to the general orchestration of the simulation
 */

#pragma once

/**
 * @brief Process used to orchestrate the simulation
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int uds_server_process(int argc, char *argv[]);

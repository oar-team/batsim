/**
 * @file workload.hpp
 * @brief Contains workload-related functions
 */

#pragma once

#include <string>

struct BatsimContext;

/**
 * @brief Checks whether a workload is valid
 * @param[in] context The BatsimContext
 */
void check_worload_validity(const BatsimContext * context);

/**
 * @brief Loads a workload from a JSON file
 * @param[in,out] context The BatsimContext
 * @param[in] json_filename The name of the JSON workload file
 * @param[out] nb_machines The number of machines described in the JSON file
 */
void load_json_workload(BatsimContext * context, const std::string & json_filename, int & nb_machines);

/**
 * @brief Used to simulate SMPI jobs
 * @param[in] context The BatsimContext
 */
void register_smpi_applications(const BatsimContext * context);

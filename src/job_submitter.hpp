/**
 * @file job_submitter.hpp
 * @brief Contains functions related to job submission
 */

#pragma once

#include <string>

struct BatsimContext;

/**
 * @brief The process in charge of submitting static jobs (those described before running the simulations)
 * @param argc The number of arguments
 * @param argv The argument values
 * @return 0
 */
int static_job_submitter_process(int argc, char *argv[]);

/**
 * @brief The process in charge of submitting dynamic jobs that are part of a workflow
 * @param argc The number of arguments
 * @param argv The argument values
 * @return 0
 */
int workflow_submitter_process(int argc, char *argv[]);

/**
 * @brief Execute jobs sequentially without server and scheduler
 * @param[in] context The BatsimContext
 * @param[in] workload_name The name of the workload attached to the submitter
 */
void batexec_job_launcher_process(BatsimContext * context,
                                  std::string workload_name);

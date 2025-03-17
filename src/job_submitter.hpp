/**
 * @file job_submitter.hpp
 * @brief Contains functions related to job submission
 */

#pragma once

#include <string>

struct BatsimContext;

/**
 * @brief The process in charge of submitting static jobs (those described before running the simulations)
 * @param[in] context The BatsimContext
 * @param[in] workload_name The name of the workload attached to the submitter
 */
void static_job_submitter_process(BatsimContext * context,
                                  std::string workload_name);

/**
 * @brief The process in charge of submitting dynamic jobs that are part of a workflow
 * @param[in] context The BatsimContext
 * @param[in] workflow_name The name of the workflow attached to the submitter
 */
void workflow_submitter_process(BatsimContext * context,
                                std::string workflow_name);

/**
 * @file job_submitter.hpp
 * @brief Contains functions related to job submission
 */

#pragma once

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
 * @param[in] argc The number of arguments
 * @param[in] argv The argument values
 * @return 0
 */
int job_launcher_process(int argc, char *argv[]);

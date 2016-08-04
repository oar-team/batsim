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

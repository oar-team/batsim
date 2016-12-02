/**
 * @file jobs_execution.hpp
 * @brief Contains functions related to the execution of the jobs
 */

#pragma once

#include "ipp.hpp"
#include "context.hpp"

/**
 * @brief The process in charge of killing a job if it reaches its walltime
 * @param argc The number of arguments
 * @param argv The arguments' values
 * @return 0
 */
int killer_process(int argc, char *argv[]);

/**
 * @brief The process in charge of executing a SMPI job
 * @param argc The number of arguments
 * @param argv The arguments's values
 * @return 0
 */
int smpi_replay_process(int argc, char *argv[]);

/**
 * @brief Executes the profile of a job
 * @param[in] context The Batsim Context
 * @param[in] profile_name The name of the profile to execute
 * @param[in] allocation The machines the job should be executed on
 * @param[in,out] remaining_time The remaining amount of time before walltime
 * @return 0
 */
int execute_profile(BatsimContext *context,
                    const std::string & profile_name,
                    const SchedulingAllocation * allocation,
                    double * remaining_time);

/**
 * @brief Simple process to execute a job
 * @param argc unused?
 * @param argv unused?
 * @return 0
 */
int lite_execute_job_process(int argc, char *argv[]);

/**
 * @brief The process in charge of executing a job
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int execute_job_process(int argc, char *argv[]);

/**
 * @brief The process in charge of waiting for a given amount of time (related to the NOPMeLater message)
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int waiter_process(int argc, char *argv[]);

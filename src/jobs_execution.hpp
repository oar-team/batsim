/**
 * @file jobs_execution.hpp
 * @brief Contains functions related to the execution of the jobs
 */

#pragma once

#include "ipp.hpp"
#include "context.hpp"

/**
 * @brief Structure used to clean the data of the execute_profile function if the process executing it gets killed
 * @details This structure should be allocated (via new!) before running execute_profile (but not when doing recursive calls). It is freed by execute_profile_cleanup.
 */
struct CleanExecuteProfileData
{
    double * computation_amount = nullptr; //!< The computation amount (may be null)
    double * communication_amount = nullptr; //!< The communication amount (may be null)
    ExecuteJobProcessArguments * exec_process_args = nullptr; //!< The ExecuteJobProcessArguments
};

/**
 * @brief The process in charge of killing a job if it reaches its walltime
 * @param argc The number of arguments
 * @param argv The arguments values
 * @return 0
 */
int killer_process(int argc, char *argv[]);

/**
 * @brief The process in charge of executing a SMPI job
 * @param argc The number of arguments
 * @param argv The arguments values
 * @return 0
 */
int smpi_replay_process(int argc, char *argv[]);

/**
 * @brief Executes the profile of a job
 * @param[in] context The Batsim Context
 * @param[in] profile_name The name of the profile to execute
 * @param[in] allocation The machines the job should be executed on
 * @param[in,out] cleanup_data The data to clean on bad process termination (kill)
 * @param[in,out] remaining_time The remaining amount of time before walltime
 * @return 0
 */
int execute_profile(BatsimContext *context,
                    const std::string & profile_name,
                    const SchedulingAllocation * allocation,
                    CleanExecuteProfileData * cleanup_data,
                    double * remaining_time);

/**
 * @brief Is executed on execute_profile termination, allows to clean memory on kill
 * @param[in] unknown An unknown argument (oops?)
 * @param[in] data The data to free
 * @return 0
 */
int execute_profile_cleanup(void * unknown, void * data);

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
 * @param[in] argv The arguments values
 * @return 0
 */
int execute_job_process(int argc, char *argv[]);

/**
 * @brief The process in charge of waiting for a given amount of time (related to the NOPMeLater message)
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments values
 * @return 0
 */
int waiter_process(int argc, char *argv[]);

/**
 * @brief The process in charge of killing a given job
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments values
 * @return 0
 */
int killer_process(int argc, char *argv[]);

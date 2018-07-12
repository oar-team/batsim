/**
 * @file jobs_execution.hpp
 * @brief Contains functions related to the execution of the jobs
 */

#pragma once

#include "ipp.hpp"
#include "context.hpp"

/**
 * @brief Structure used to clean the data of a profile execution if the process executing it gets killed
 * @details This structure should be allocated (via new!) before running execute_task (but not when doing recursive calls). It is freed by execute_task_cleanup.
 */
struct CleanExecuteTaskData
{
    double * computation_amount = nullptr; //!< The computation amount (may be null)
    double * communication_amount = nullptr; //!< The communication amount (may be null)
    ExecuteJobProcessArguments * exec_process_args = nullptr; //!< The ExecuteJobProcessArguments
    msg_task_t task = nullptr; //!< The task
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
 * @brief Simulates a delay profile (sleeps until finished or walltime)
 * @param[in] sleeptime The time to sleep
 * @param[in,out] remaining_time The remaining amount of time before walltime
 * @return 0 if enough time is available, -1 in the case of a timeout
 */
int do_delay_task(double sleeptime, double * remaining_time);

/**
 * @brief Execute a BatTask recursively regarding on its profile type
 * @param[in,out] btask the task to execute
 * @param[in] context usefull information about Batsim context
 * @param[in] allocation the host to execute the task to
 * @param[in,out] cleanup_data to give to simgrid cleanup hook
 * @param[in,out] remaining_time remaining time of the current task
 * @return the profile return code if successful, -1 if walltime reached
 */
int execute_task(BatTask * btask,
                 BatsimContext *context,
                 const SchedulingAllocation * allocation,
                 CleanExecuteTaskData * cleanup_data,
                 double * remaining_time);

/**
 * @brief Is executed on execute_task termination, allows to clean memory on kill
 * @param[in] unknown An unknown argument (oops?)
 * @param[in] data The data to free
 * @return 0
 */
int execute_task_cleanup(void * unknown, void * data);

/**
 * @brief The process in charge of executing a job
 * @param context The BatsimContext
 * @param allocation The job allocation
 * @param notify_server_at_end Whether a message to the server must be sent after job completion
 * @param io_profile The optional IO profile
 */
void execute_job_process(BatsimContext *context, SchedulingAllocation *allocation, bool notify_server_at_end, Profile *io_profile);

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

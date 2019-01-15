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
    msg_task_t task = nullptr; //!< The task
};

/**
 * @brief The process in charge of killing a job if it reaches its walltime
 * @param[in] context The BatsimContext
 * @param[in] jobs_ids The ids of the jobs to kill
 */
void killer_process(BatsimContext *context, std::vector<JobIdentifier> jobs_ids);

/**
 * @brief The process in charge of executing a rank of a SMPI profile
 * @param[in] job The job associated with this profile execution
 * @param[in] profile_data The data associated with the executed profile
 * @param[in,out] barrier The barrier associated with this job
 * @param[in] rank The rank whose replay is to be simulated
 */
void smpi_replay_process(Job* job, SmpiProfileData * profile_data, simgrid::s4u::BarrierPtr barrier, int rank);

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
 * @param[in] target_time The time at which the waiter should stop waiting
 * @param[in] server_data The ServerData. Used to check whether the simulation is finished or not
 */
void waiter_process(double target_time, const ServerData *server_data);

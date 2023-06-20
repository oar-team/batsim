/**
 * @file jobs_execution.hpp
 * @brief Contains functions related to the execution of the jobs
 */

#pragma once

#include "ipp.hpp"
#include "context.hpp"


/**
 * @brief The process in charge of killing a job if it reaches its walltime
 * @param[in] context The BatsimContext
 * @param[in] kill_jobs_msg The message that initiated the kills
 * @param[in] killed_job_state The JobState to be set for every killed Job
 * @param[in] acknowledge_kill_on_protocol Whether to acknwoledge the kill to the decision process through the protocol
 */
void killer_process(
    BatsimContext *context,
    KillJobsMessage * kill_jobs_msg,
    JobState killed_job_state = JobState::JOB_STATE_COMPLETED_KILLED,
    bool acknowledge_kill_on_protocol = true
);

/**
 * @brief The process in charge of executing a rank of a SMPI profile
 * @param[in] job The job associated with this profile execution
 * @param[in] profile_data The data associated with the executed profile
 * @param[in] termination_mbox_name The name of the mailbox onto which this function writes to signal its termination.
 * @param[in] rank The rank whose replay is to be simulated
 */
void smpi_replay_process(JobPtr job, ReplaySmpiProfileData * profile_data, const std::string & termination_mbox_name, int rank);

/**
 * @brief Simulates a delay profile (sleeps until finished or walltime)
 * @param[in] sleeptime The time to sleep
 * @param[in,out] remaining_time The remaining amount of time before walltime
 * @return 0 if enough time is available, -1 in the case of a timeout
 */
int do_delay_task(double sleeptime, double * remaining_time);

/**
 * @brief Execute a BatTask recursively regarding on its profile type
 * @param[in,out] btask The task to execute
 * @param[in] context The BatsimContext
 * @param[in] execute_job_msg User-given information about how the task should be executed
 * @param[in,out] remaining_time remaining time of the current task
 * @return the profile return code if successful, -1 if walltime reached, -2 if parallel task has been cancelled
 */
int execute_task(
    BatTask * btask,
    BatsimContext *context,
    const std::shared_ptr<ExecuteJobMessage> & execute_job_msg,
    double * remaining_time
);

/**
 * @brief The process in charge of executing a job
 * @param context The BatsimContext
 * @param job The job to execute
 * @param notify_server_at_end Whether a message to the server must be sent after job completion
 */
void execute_job_process(
    BatsimContext *context,
    JobPtr job,
    bool notify_server_at_end
);

/**
 * @brief The process in charge of waiting for a given amount of time (related to the NOPMeLater message)
 * @param[in] target_time The time at which the waiter should stop waiting
 * @param[in] server_data The ServerData. Used to check whether the simulation is finished or not
 */
void waiter_process(double target_time, const ServerData *server_data);

/**
 * @brief Cancels the ptasks associated with this BatTask (recursively), if any
 * @param[in] btask The BatTask whose ptask should be cancelled
 * @return True if one or more ptasks have been cancelled by this call, false otherwise
 */
bool cancel_ptasks(BatTask * btask);

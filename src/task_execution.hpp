#pragma once

#include "context.hpp"
#include "ipp.hpp"
#include "jobs.hpp"

/**
 * @brief Execute tasks that correspond to parallel task (MSG) profiles
 * @param[in,out] btask The task to execute. Progress information is stored within it.
 * @param[in] allocation The hosts where the task can be executed
 * @param[in,out] remaining_time The remaining time of the current task. It will be automatically killed if 0 is reached.
 * @param[in] context The BatsimContext
 * @param[in,out] cleanup_data Used to clean data on process kill (happens when the scheduler requests jobs to be killed).
 * @return The profile return code on success, -1 on timeout (remaining time reached 0)
 */
int execute_msg_task(BatTask * btask,
                     const SchedulingAllocation* allocation,
                     double * remaining_time,
                     BatsimContext * context,
                     CleanExecuteTaskData * cleanup_data);

#pragma once

#include "context.hpp"
#include "ipp.hpp"
#include "jobs.hpp"

/**
 * @brief Execute tasks from profiles that use MSG simgrid model
 * @param[in,out] btask task to be filled and compute
 * @param[in] allocation the host to execute the task to
 * @param[in] nb_res the number of resources the task have to run on
 * @param[in,out] remaining_time remaining time of the current task
 * @param[in] context usefull information about Batsim context
 * @param[in,out] cleanup_data The data to clean on bad process termination (kill)
 * @return the task profile return code of -1 it the task timeout
 */
int execute_msg_task(
        BatTask * btask,
        const SchedulingAllocation* allocation,
        unsigned int nb_res,
        double * remaining_time,
        BatsimContext * context,
        CleanExecuteProfileData * cleanup_data);

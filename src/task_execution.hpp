#pragma once

#include "context.hpp"
#include "ipp.hpp"
#include "jobs.hpp"

/**
 * @brief Execute tasks from profiles that use MSG simgrid model
 */
int execute_msg_task(
        BatTask * btask,
        const SchedulingAllocation* allocation,
        int nb_res,
        double * remaining_time,
        BatsimContext * context,
        CleanExecuteProfileData * cleanup_data);

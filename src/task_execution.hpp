#pragma once

#include "context.hpp"
#include "ipp.hpp"

/**
 * @brief Execute tasks from profiles that use MSG simgrid model
 */
int execute_msg_task(
        const SchedulingAllocation* allocation,
        int nb_res,
        Job * job,
        double * remaining_time,
        Profile * profile,
        BatsimContext * context,
        CleanExecuteProfileData * cleanup_data);

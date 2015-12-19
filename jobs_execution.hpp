#pragma once

#include "ipp.hpp"
#include "batsim.h"
#include "context.hpp"

int killer_process(int argc, char *argv[]);
int smpi_replay_process(int argc, char *argv[]);

int execute_profile(BatsimContext * context,
                    const std::string & profile_name,
                    SchedulingAllocation * allocation,
                    double * remaining_time);

int execute_job_process(int argc, char *argv[]);

int waiter_process(int argc, char *argv[]);

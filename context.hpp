/**
 * @file context.hpp The Batsim context
 */

#pragma once

#include "network.hpp"
#include "machines.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "export.hpp"

struct BatsimContext
{
    UnixDomainSocket socket;
    Machines machines;
    Jobs jobs;
    Profiles profiles;
    PajeTracer tracer;

    long long microseconds_used_by_scheduler = 0;
    bool energy_used;
    bool allow_space_sharing;
    std::string platform_filename;
    std::string workload_filename;
};

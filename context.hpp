/**
 * @file context.hpp The Batsim context
 */

#pragma once

#include "network.hpp"
#include "machines.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "export.hpp"
#include "pstate.hpp"

struct BatsimContext
{
    UnixDomainSocket socket;
    Machines machines;
    Jobs jobs;
    Profiles profiles;
    PajeTracer paje_tracer;
    PStateChangeTracer pstate_tracer;
    CurrentSwitches current_switches;

    long long microseconds_used_by_scheduler = 0;
    bool energy_used;
    bool smpi_used;
    bool allow_space_sharing;
    bool trace_schedule;
    std::string platform_filename;
    std::string workload_filename;
    std::string export_prefix;
};

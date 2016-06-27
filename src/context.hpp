/**
 * @file context.hpp
 * @brief The Batsim context
 */

#pragma once

#include "network.hpp"
#include "machines.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "export.hpp"
#include "pstate.hpp"

/**
 * @brief The Batsim context
 */
struct BatsimContext
{
    UnixDomainSocket socket;                        //!< The UnixDomainSocket
    Machines machines;                              //!< The machines
    Jobs jobs;                                      //!< The jobs
    Profiles profiles;                              //!< The profiles
    PajeTracer paje_tracer;                         //!< The PajeTracer
    PStateChangeTracer pstate_tracer;               //!< The PStateChangeTracer
    EnergyConsumptionTracer energy_tracer;          //!< The EnergyConsumptionTracer
    CurrentSwitches current_switches;               //!< The current switches

    long long microseconds_used_by_scheduler = 0;   //!< The number of microseconds used by the scheduler
    bool energy_used;                               //!< Stores whether the energy part of Batsim should be used
    bool smpi_used;                                 //!< Stores whether SMPI should be used
    bool allow_space_sharing;                       //!< Stores whether space sharing (using the same machines to compute different jobs) should be allowed
    bool trace_schedule;                            //!< Stores whether the resulting schedule should be outputted
    std::string platform_filename;                  //!< The name of the platform file
    std::string workload_filename;                  //!< The name of the workload file
    std::string export_prefix;                      //!< The output export prefix
};

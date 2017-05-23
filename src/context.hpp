/**
 * @file context.hpp
 * @brief The Batsim context
 */

#pragma once

#include <chrono>
#include <vector>

#include <zmq.hpp>

#include <rapidjson/document.h>

#include "exact_numbers.hpp"
#include "export.hpp"
#include "jobs.hpp"
#include "machines.hpp"
#include "network.hpp"
#include "profiles.hpp"
#include "protocol.hpp"
#include "pstate.hpp"
#include "storage.hpp"
#include "workflow.hpp"
#include "workload.hpp"

/**
 * @brief Stores a high-resolution timestamp
 */
typedef std::chrono::time_point<std::chrono::high_resolution_clock> my_timestamp;

/**
 * @brief The Batsim context
 */
struct BatsimContext
{
    zmq::context_t zmq_context;                     //!< The Zero MQ context
    zmq::socket_t * zmq_socket = nullptr;           //!< The Zero MQ socket (REQ)
    AbstractProtocolReader * proto_reader = nullptr;//!< The protocol reader
    AbstractProtocolWriter * proto_writer = nullptr;//!< The protocol writer

    Machines machines;                              //!< The machines
    Workloads workloads;                            //!< The workloads
    Workflows workflows;                            //!< The workflows
    PajeTracer paje_tracer;                         //!< The PajeTracer
    PStateChangeTracer pstate_tracer;               //!< The PStateChangeTracer
    EnergyConsumptionTracer energy_tracer;          //!< The EnergyConsumptionTracer
    MachineStateTracer machine_state_tracer;        //!< The MachineStateTracer
    CurrentSwitches current_switches;               //!< The current switches

    RedisStorage storage;                           //!< The RedisStorage

    rapidjson::Document config_file;                //!< The configuration file
    bool redis_enabled;                             //!< Stores whether Redis should be used
    bool submission_forward_profiles;               //!< Stores whether the profile information of jobs should be sent to the scheduler
    bool submission_sched_enabled;                  //!< Stores whether the scheduler will be able to send jobs along the simulation
    bool submission_sched_finished = false;         //!< Stores whether the scheduler has finished submitting jobs.
    bool submission_sched_ack;                      //!< Stores whether Batsim will acknowledge dynamic job submission (emit JOB_SUBMITTED events)

    bool terminate_with_last_workflow;              //!< If true, allows to ignore the jobs submitted after the last workflow termination

    Rational energy_first_job_submission = -1;      //!< The amount of consumed energy (J) when the first job is submitted
    Rational energy_last_job_completion = -1;       //!< The amount of consumed energy (J) when the last job is completed

    Rational microseconds_used_by_scheduler = 0;    //!< The number of microseconds used by the scheduler
    my_timestamp simulation_start_time;             //!< The moment in time at which the simulation has started
    my_timestamp simulation_end_time;               //!< The moment in time at which the simulation has ended

    unsigned long long nb_machine_switches = 0;     //!< The number of machine switches done in the simulation (should be greater or equal to SET_RESOURCE_STATE events. Equal if all requested switches only concern single machines). Does not count transition states.
    unsigned long long nb_grouped_switches = 0;     //!< The number of switches done in the simulation (should equal to the number of received SET_RESOURCE_STATE events). Does not count transition states.

    bool energy_used;                               //!< Stores whether the energy part of Batsim should be used
    bool smpi_used;                                 //!< Stores whether SMPI should be used
    bool allow_time_sharing;                        //!< Stores whether time sharing (using the same machines to compute different jobs) should be allowed
    bool trace_schedule;                            //!< Stores whether the resulting schedule should be outputted
    bool trace_machine_states;                      //!< Stores whether the machines states should be outputted
    std::string platform_filename;                  //!< The name of the platform file
    std::string export_prefix;                      //!< The output export prefix
    int workflow_nb_concurrent_jobs_limit;          //!< Limits the number of concurrent jobs for workflows
};

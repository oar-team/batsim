/**
 * @file context.hpp
 * @brief The Batsim context
 */

#pragma once

#include <chrono>
#include <vector>

#include <zmq.h>

#include <rapidjson/document.h>

#include <batprotocol.hpp>

#include "events.hpp"
#include "export.hpp"
#include "jobs.hpp"
#include "machines.hpp"
#include "network.hpp"
#include "profiles.hpp"
#include "protocol.hpp"
#include "pstate.hpp"
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
    void * zmq_context = nullptr;                   //!< The Zero MQ context
    void * zmq_socket = nullptr;                    //!< The Zero MQ socket (REQ)
    AbstractProtocolReader * proto_reader = nullptr;//!< The protocol reader
    batprotocol::MessageBuilder * proto_msg_builder = nullptr; //!< The batprotocol message builder
    MainArguments * main_args = nullptr;            //!< The arguments received by Batsim's main

    Machines machines;                              //!< The machines
    Workloads workloads;                            //!< The workloads
    Workflows workflows;                            //!< The workflows
    std::map<std::string, EventList*> event_lists;  //!< The map of EventLists
    PajeTracer paje_tracer;                         //!< The PajeTracer
    PStateChangeTracer pstate_tracer;               //!< The PStateChangeTracer
    EnergyConsumptionTracer energy_tracer;          //!< The EnergyConsumptionTracer
    MachineStateTracer machine_state_tracer;        //!< The MachineStateTracer
    JobsTracer jobs_tracer;                         //!< The JobsTracer
    CurrentSwitches current_switches;               //!< The current switches

    rapidjson::Document config_json;                //!< The configuration information sent to the scheduler
    bool submission_forward_profiles;               //!< Stores whether the profile information of submitted jobs should be sent to the scheduler
    bool registration_sched_enabled;                //!< Stores whether the scheduler will be able to register jobs and profiles during the simulation
    bool registration_sched_finished = false;       //!< Stores whether the scheduler has finished submitting jobs.
    bool registration_sched_ack;                    //!< Stores whether Batsim will acknowledge dynamic job submission (emit JOB_SUBMITTED events)
    bool garbage_collect_profiles = true;           //!< Stores whether Batsim will garbage collect the Profiles.

    bool terminate_with_last_workflow;              //!< If true, allows to ignore the jobs submitted after the last workflow termination

    long double energy_first_job_submission = -1;   //!< The amount of consumed energy (J) when the first job is submitted
    long double energy_last_job_completion = -1;    //!< The amount of consumed energy (J) when the last job is completed

    long double microseconds_used_by_scheduler = 0; //!< The number of microseconds used by the scheduler
    my_timestamp simulation_start_time;             //!< The moment in time at which the simulation has started
    my_timestamp simulation_end_time;               //!< The moment in time at which the simulation has ended

    unsigned long long nb_machine_switches = 0;     //!< The number of machine switches done in the simulation (should be greater or equal to SET_RESOURCE_STATE events. Equal if all requested switches only concern single machines). Does not count transition states.
    unsigned long long nb_grouped_switches = 0;     //!< The number of switches done in the simulation (should equal to the number of received SET_RESOURCE_STATE events). Does not count transition states.

    bool energy_used;                               //!< Stores whether the energy part of Batsim should be used
    bool smpi_used;                                 //!< Stores whether SMPI should be used
    bool allow_compute_sharing;                     //!< Stores whether sharing (using the same machine to run different jobs concurrently) should be allowed on compute machines
    bool allow_storage_sharing;                     //!< Stores whether sharing (using the same machine to run different jobs concurrently) should be allowed on storage machines
    bool trace_schedule;                            //!< Stores whether the resulting schedule should be outputted
    bool trace_machine_states;                      //!< Stores whether the machines states should be outputted
    std::string platform_filename;                  //!< The name of the platform file
    std::string export_prefix;                      //!< The output export prefix
    int workflow_nb_concurrent_jobs_limit;          //!< Limits the number of concurrent jobs for workflows

    std::string batsim_version;                     //!< The Batsim version (got from the BATSIM_VERSION variable that is usually set by the build system)

    ~BatsimContext();
};

/**
 * @file batsim.cpp
 * @brief Batsim's entry point
 */

/** @def BATSIM_VERSION
 *  @brief What batsim --version should return.
 *
 *  This is usually set by Batsim's build system (Meson, via Nix or not)
**/
#ifndef BATSIM_VERSION
    #define BATSIM_VERSION vUNKNOWN
#endif


#include <stdio.h>
#include <unistd.h>

#include <string>
#include <fstream>
#include <functional>
#include <streambuf>

#include <simgrid/s4u.hpp>
#include <smpi/smpi.h>
#include <simgrid/plugins/energy.h>
#include <simgrid/version.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include "batsim.hpp"
#include "context.hpp"
#include "event_submitter.hpp"
#include "events.hpp"
#include "export.hpp"
#include "ipp.hpp"
#include "job_submitter.hpp"
#include "jobs.hpp"
#include "jobs_execution.hpp"
#include "machines.hpp"
#include "profiles.hpp"
#include "protocol.hpp"
#include "server.hpp"
#include "task_execution.hpp"
#include "workload.hpp"
#include "workflow.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "batsim"); //!< Logging

std::string MainArguments::generate_execution_context_json() const
{
    using namespace rapidjson;
    Document object;
    auto & alloc = object.GetAllocator();
    object.SetObject();

    // Generate the content to dump
    object.AddMember("socket_endpoint", Value().SetString(this->edc_socket_endpoint.c_str(), alloc), alloc);
    object.AddMember("export_prefix", Value().SetString(this->export_prefix.c_str(), alloc), alloc);
    object.AddMember("external_scheduler", Value().SetBool(this->program_type == ProgramType::BATSIM), alloc);

    // Dump the object to a string
    StringBuffer buffer;
    rapidjson::Writer<StringBuffer> writer(buffer);
    object.Accept(writer);

    return buffer.GetString();
}

void configure_batsim_logging_output(const MainArguments & main_args)
{
    vector<string> log_categories_to_set = {
        "batsim",
        "edc",
        "events",
        "event_submitter",
        "export",
        "ipp",
        "jobs",
        "jobs_execution",
        "job_submitter",
        "machines",
        "profiles",
        "protocol",
        "pstate",
        "server",
        "task_execution",
        "workflow",
        "workload"
    };
    string log_threshold_to_set = "critical";

    if (main_args.verbosity == VerbosityLevel::QUIET)
    {
        log_threshold_to_set = "error";
    }
    else if (main_args.verbosity == VerbosityLevel::DEBUG)
    {
        log_threshold_to_set = "debug";
    }
    else if (main_args.verbosity == VerbosityLevel::INFORMATION)
    {
        log_threshold_to_set = "info";
    }
    else
    {
        xbt_assert(false, "FIXME!");
    }

    for (const auto & log_cat : log_categories_to_set)
    {
        const string final_str = log_cat + ".thresh:" + log_threshold_to_set;
        xbt_log_control_set(final_str.c_str());
    }

    // Batsim is always set to info, to allow to trace Batsim's input easily
    xbt_log_control_set("batsim.thresh:info");

    // Simgrid-related log control
    xbt_log_control_set("surf_energy.thresh:critical");
}

void load_workloads_and_workflows(const MainArguments & main_args, BatsimContext * context, int & max_nb_machines_to_use)
{
    int max_nb_machines_in_workloads = -1;

    // Create the workloads
    for (const MainArguments::WorkloadDescription & desc : main_args.workload_descriptions)
    {
        XBT_INFO("Workload '%s' corresponds to workload file '%s'.", desc.name.c_str(), desc.filename.c_str());
        Workload * workload = Workload::new_static_workload(desc.name, desc.filename);

        int nb_machines_in_workload = -1;
        workload->load_from_json(desc.filename, nb_machines_in_workload);
        max_nb_machines_in_workloads = std::max(max_nb_machines_in_workloads, nb_machines_in_workload);

        context->workloads.insert_workload(desc.name, workload);
    }

    // Create the workflows
    for (const MainArguments::WorkflowDescription & desc : main_args.workflow_descriptions)
    {
        XBT_INFO("Workflow '%s' corresponds to workflow file '%s'.", desc.name.c_str(), desc.filename.c_str());
        Workload * workload = Workload::new_static_workload(desc.workload_name, desc.filename);
        workload->jobs = new Jobs;
        workload->profiles = new Profiles;
        workload->jobs->set_workload(workload);
        workload->jobs->set_profiles(workload->profiles);
        context->workloads.insert_workload(desc.workload_name, workload);

        Workflow * workflow = new Workflow(desc.name);
        workflow->start_time = desc.start_time;
        workflow->load_from_xml(desc.filename);
        context->workflows.insert_workflow(desc.name, workflow);
    }

    // Let's compute how the number of machines to use should be limited
    max_nb_machines_to_use = -1;
    if ((main_args.limit_machines_count_by_workload) && (main_args.limit_machines_count > 0))
    {
        max_nb_machines_to_use = std::min(static_cast<int>(main_args.limit_machines_count), max_nb_machines_in_workloads);
    }
    else if (main_args.limit_machines_count_by_workload)
    {
        max_nb_machines_to_use = max_nb_machines_in_workloads;
    }
    else if (main_args.limit_machines_count > 0)
    {
        max_nb_machines_to_use = main_args.limit_machines_count;
    }

    if (max_nb_machines_to_use != -1)
    {
        XBT_INFO("The maximum number of machines to use is %d.", max_nb_machines_to_use);
    }
}

void load_eventLists(const MainArguments & main_args, BatsimContext * context)
{
    for (const MainArguments::EventListDescription & desc : main_args.eventList_descriptions)
    {
        XBT_INFO("Event list '%s' corresponds to events file '%s'.", desc.name.c_str(), desc.filename.c_str());
        auto events = new EventList(desc.name, true);
        events->load_from_json(desc.filename, false);
        context->event_lists[desc.name] = events;
    }
}

void start_initial_simulation_processes(const MainArguments & main_args,
                                        BatsimContext * context,
                                        bool is_batexec)
{
    const Machine * master_machine = context->machines.master_machine();

    // Let's run a static_job_submitter process for each workload
    for (const MainArguments::WorkloadDescription & desc : main_args.workload_descriptions)
    {
        string submitter_instance_name = "workload_submitter_" + desc.name;

        XBT_DEBUG("Creating a workload_submitter process...");
        auto actor_function = static_job_submitter_process;
        if (is_batexec)
        {
            actor_function = batexec_job_launcher_process;
        }

        simgrid::s4u::Actor::create(submitter_instance_name.c_str(),
                                    master_machine->host,
                                    actor_function,
                                    context, desc.name);
        XBT_INFO("The process '%s' has been created.", submitter_instance_name.c_str());
    }

    // Let's run a workflow_submitter process for each workflow
    for (const MainArguments::WorkflowDescription & desc : main_args.workflow_descriptions)
    {
        XBT_DEBUG("Creating a workflow_submitter process...");
        string submitter_instance_name = "workflow_submitter_" + desc.name;
        simgrid::s4u::Actor::create(submitter_instance_name.c_str(),
                                    master_machine->host,
                                    workflow_submitter_process,
                                    context, desc.name);
        XBT_INFO("The process '%s' has been created.", submitter_instance_name.c_str());
    }

    // Let's run a static_event_submitter process for each list of event
    for (const MainArguments::EventListDescription & desc : main_args.eventList_descriptions)
    {
        string submitter_instance_name = "event_submitter_" + desc.name;

        XBT_DEBUG("Creating an event_submitter process...");
        auto actor_function = static_event_submitter_process;
        simgrid::s4u::Actor::create(submitter_instance_name.c_str(),
                                    master_machine->host,
                                    actor_function,
                                    context, desc.name);
        XBT_INFO("The process '%s' has been created.", submitter_instance_name.c_str());
    }

    if (!is_batexec)
    {
        XBT_DEBUG("Creating the 'server' process...");
        simgrid::s4u::Actor::create("server", master_machine->host,
                                    server_process, context);
        XBT_INFO("The process 'server' has been created.");
    }
}

/**
 * @brief Main function
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0 on success, something else otherwise
 */
int main(int argc, char * argv[])
{
    // Let's parse command-line arguments
    MainArguments main_args;
    int return_code = 1;
    bool run_simulation = false;
    bool only_print_information = false;

    parse_main_args(argc, argv, main_args, return_code, run_simulation, only_print_information);

    // Print the requested information and exit if this is requested by user
    if (only_print_information)
    {
        if (main_args.print_simgrid_version)
        {
            int sg_major, sg_minor, sg_patch;
            sg_version_get(&sg_major, &sg_minor, &sg_patch);
            printf("%d.%d.%d\n", sg_major, sg_minor, sg_patch);
        }
        else if (main_args.dump_execution_context)
        {
            auto execution_context_json = main_args.generate_execution_context_json();
            printf("%s\n", execution_context_json.c_str());
        }
        else if (main_args.print_batsim_version)
        {
            printf("%s\n", STR(BATSIM_VERSION));
        }
        else if (main_args.print_batsim_commit)
        {
            printf("Printing Batsim git commit is not implemented.\n");
            return_code = 1;
        }
        else if (main_args.print_simgrid_commit)
        {
            printf("Printing SimGrid git commit is not implemented.\n");
            return_code = 1;
        }

        fflush(stdout);
    }

    if (!run_simulation)
        return return_code;

    // Let's configure how Batsim should be logged
    configure_batsim_logging_output(main_args);

    // Initialize the energy plugin before creating the engine
    if (main_args.host_energy_used)
    {
        sg_host_energy_plugin_init();
    }

    // Instantiate SimGrid
    simgrid::s4u::Engine engine(&argc, argv);

    // Setting SimGrid configuration options, if any
    for (const string & cfg_string : main_args.simgrid_config)
    {
        engine.set_config(cfg_string);
    }

    // Setting SimGrid logging options, if any
    for (const string & log_string : main_args.simgrid_logging)
    {
        xbt_log_control_set(log_string.c_str());
    }

    // Let's create the BatsimContext, which stores information about the current instance
    BatsimContext context;
    set_configuration(&context, main_args);

    context.batsim_version = STR(BATSIM_VERSION);
    XBT_INFO("Batsim version: %s", context.batsim_version.c_str());

    // Let's load the workloads and workflows
    int max_nb_machines_to_use = -1;
    load_workloads_and_workflows(main_args, &context, max_nb_machines_to_use);

    // Let's load the eventLists
    load_eventLists(main_args, &context);

    // initialyse Ptask L07 model
    engine.set_config("host/model:ptask_L07");

    // Let's choose which SimGrid computing model should be used
    XBT_INFO("Checking whether SMPI is used or not...");
    context.smpi_used = context.workloads.contains_smpi_job(); // todo: SMPI workflows

    if (context.smpi_used)
    {
        XBT_INFO("SMPI will be used.");
        context.workloads.register_smpi_applications(); // todo: SMPI workflows
        SMPI_init();
    }

    // Register Batsim replay functions
    xbt_replay_action_register("m_usage", usage_trace_replayer);

    // Let's create the machines
    create_machines(main_args, &context, max_nb_machines_to_use);

    // Prepare Batsim's outputs
    prepare_batsim_outputs(&context);

    if (main_args.program_type == ProgramType::BATSIM)
    {
        context.edc_json_format = main_args.edc_json_format;
        if (!main_args.edc_socket_endpoint.empty())
        {
            // Create a ZeroMQ context
            context.zmq_context = zmq_ctx_new();

            // Create and connect the socket
            context.edc = ExternalDecisionComponent::new_process(context.zmq_context, main_args.edc_socket_endpoint);
        }
        else
        {
            // Load the external library
            context.edc = ExternalDecisionComponent::new_library(main_args.edc_library_path, main_args.edc_library_load_method);
        }

        // Generate initialization flags
        uint8_t flags = 0;
        if (main_args.edc_json_format)
            flags |= 0x2;
        else
            flags |= 0x1;
        context.edc->init((const uint8_t*)main_args.edc_init_buffer.data(), main_args.edc_init_buffer.size(), flags);

        // Create the protocol message manager
        context.proto_msg_builder = new batprotocol::MessageBuilder(true);

        // Let's execute the initial processes
        start_initial_simulation_processes(main_args, &context);
    }
    else if (main_args.program_type == ProgramType::BATEXEC)
    {
        // Let's execute the initial processes
        start_initial_simulation_processes(main_args, &context, true);
    }

    // Simulation main loop, handled by s4u
    engine.run();

    delete context.edc;
    context.edc = nullptr;

    zmq_ctx_destroy(context.zmq_context);
    context.zmq_context = nullptr;

    delete context.proto_msg_builder;
    context.proto_msg_builder = nullptr;

    // If SMPI had been used, it should be finalized
    if (context.smpi_used)
    {
        SMPI_finalize();
    }

    // Let's finalize Batsim's outputs
    finalize_batsim_outputs(&context);

    return 0;
}

void set_configuration(BatsimContext *context,
                       MainArguments & main_args)
{
    using namespace rapidjson;

    // ********************************************************
    // Let's load default values from the default configuration
    // ********************************************************
    Document default_config_doc;
    xbt_assert(!default_config_doc.HasParseError(),
               "Invalid default configuration file : could not be parsed.");

    // *************************************
    // Let's update the BatsimContext values
    // *************************************
    context->main_args = &main_args;

    context->platform_filename = main_args.platform_filename;
    context->export_prefix = main_args.export_prefix;
    context->workflow_nb_concurrent_jobs_limit = main_args.workflow_nb_concurrent_jobs_limit;
    context->energy_used = main_args.host_energy_used;
    context->allow_compute_sharing = false;
    context->allow_storage_sharing = false;
    context->trace_schedule = main_args.enable_schedule_tracing;
    context->trace_machine_states = main_args.enable_machine_state_tracing;
    context->simulation_start_time = chrono::high_resolution_clock::now();
    context->terminate_with_last_workflow = main_args.terminate_with_last_workflow;

    // **************************************************************************************
    // Let's write the json object holding configuration information to send to the scheduler
    // **************************************************************************************
    context->config_json.SetObject();
    auto & alloc = context->config_json.GetAllocator();
}

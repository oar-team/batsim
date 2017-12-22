/**
 * @file batsim.cpp
 * @brief Batsim's entry point
 */

/** @def STR_HELPER(x)
 *  @brief Helper macro to retrieve the string view of a macro.
 */
#define STR_HELPER(x) #x

/** @def STR(x)
 *  @brief Macro to get a const char* from a macro
 */
#define STR(x) STR_HELPER(x)

/** @def BATSIM_VERSION
 *  @brief What batsim --version should return.
 *
 *  It is either set by CMake or set to vUNKNOWN_PLEASE_COMPILE_VIA_CMAKE
**/
#ifndef BATSIM_VERSION
    #define BATSIM_VERSION vUNKNOWN_PLEASE_COMPILE_VIA_CMAKE
#endif


#include <sys/types.h>
#include <stdio.h>
#include <argp.h>
#include <unistd.h>

#include <string>
#include <fstream>

#include <simgrid/msg.h>
#include <smpi/smpi.h>
#include <simgrid/plugins/energy.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

#include <openssl/sha.h>

#include "batsim.hpp"
#include "context.hpp"
#include "export.hpp"
#include "ipp.hpp"
#include "job_submitter.hpp"
#include "jobs.hpp"
#include "jobs_execution.hpp"
#include "machines.hpp"
#include "network.hpp"
#include "profiles.hpp"
#include "protocol.hpp"
#include "server.hpp"
#include "workload.hpp"
#include "workflow.hpp"

#include "unittest/test_main.hpp"

#include "docopt/docopt.h"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "batsim"); //!< Logging

//! Batsim default JSON configuration
string default_configuration = R"({
                               "redis": {
                                 "enabled": false,
                                 "hostname": "127.0.0.1",
                                 "port": 6379,
                                 "prefix": "default"
                               },
                               "job_submission": {
                                 "forward_profiles": false,
                                 "from_scheduler": {
                                   "enabled": false,
                                   "acknowledge": true
                                 }
                               }
                             })";

/**
 * @brief Checks whether a file exists
 * @param[in] filename The file whose existence is checked<
 * @return true if and only if filename exists
 */
bool file_exists(const std::string & filename)
{
    return boost::filesystem::exists(filename);
}

/**
 * @brief Converts a string to a VerbosityLevel
 * @param[in] str The string
 * @return The matching VerbosityLevel. An exception is thrown if str is invalid.
 */
VerbosityLevel verbosity_level_from_string(const std::string & str)
{
    if (str == "quiet")
    {
        return VerbosityLevel::QUIET;
    }
    else if (str == "network-only")
    {
        return VerbosityLevel::NETWORK_ONLY;
    }
    else if (str == "information")
    {
        return VerbosityLevel::INFORMATION;
    }
    else if (str == "debug")
    {
        return VerbosityLevel::DEBUG;
    }
    else
    {
        throw std::runtime_error("Invalid verbosity level string");
    }
}

string generate_sha1_string(std::string string_to_hash, int output_length)
{
    static_assert(sizeof(unsigned char) == sizeof(char), "sizeof(unsigned char) should equals to sizeof(char)");
    xbt_assert(output_length > 0);
    xbt_assert(output_length < SHA_DIGEST_LENGTH);

    unsigned char sha1_buf[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)string_to_hash.c_str(), string_to_hash.size(), sha1_buf);

    char * output_buf = (char *) calloc(SHA_DIGEST_LENGTH * 2 + 1, sizeof(char));
    xbt_assert(output_buf != 0, "Couldn't allocate memory");

    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        int nb_printed_char = snprintf(output_buf + 2*i, 3, "%02x", sha1_buf[i]);
        (void) nb_printed_char; // Avoids a warning if assertions are ignored
        xbt_assert(nb_printed_char == 2, "Fix me :(");
    }

    XBT_DEBUG("SHA-1 of string '%s' is '%s'\n", string_to_hash.c_str(), output_buf);

    string result(output_buf, output_length);
    xbt_assert((int)result.size() == output_length);

    free(output_buf);
    return result;
}

void parse_main_args(int argc, char * argv[], MainArguments & main_args, int & return_code,
                     bool & run_simulation, bool & run_unit_tests)
{
    static const char usage[] =
R"(A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.

Usage:
  batsim -p <platform_file> [-w <workload_file>...]
                            [-W <workflow_file>...]
                            [--WS (<cut_workflow_file> <start_time>)...]
                            [options]
  batsim --help
  batsim --version
  batsim --simgrid-version
  batsim --unittest

Input options:
  -p --platform <platform_file>     The SimGrid platform to simulate.
  -w --workload <workload_file>     The workload JSON files to simulate.
  -W --workflow <workflow_file>     The workflow XML files to simulate.
  --WS --workflow-start (<cut_workflow_file> <start_time>)... The workflow XML
                                    files to simulate, with the time at which
                                    they should be started.

Most common options:
  -m, --master-host <name>          The name of the host in <platform_file>
                                    which will be used as the RJMS management
                                    host (thus NOT used to compute jobs)
                                    [default: master_host].
  -E --energy                       Enables the SimGrid energy plugin and
                                    outputs energy-related files.

Execution context options:
  --config-file <cfg_file>          Configuration file name (optional). [default: None]
  -s, --socket-endpoint <endpoint>  The Decision process socket endpoint
                                    Decision process [default: tcp://localhost:28000].
  --redis-hostname <redis_host>     The Redis server hostname. Read from config file by default.
                                    [default: None]
  --redis-port <redis_port>         The Redis server port. Read from config file by default.
                                    [default: -1]
  --redis-prefix <prefix>           The Redis prefix. Read from config file by default.
                                    [default: None]

Output options:
  -e, --export <prefix>             The export filename prefix used to generate
                                    simulation output [default: out].
  --enable-sg-process-tracing       Enables SimGrid process tracing
  --disable-schedule-tracing        Disables the Pajé schedule outputting.
  --disable-machine-state-tracing   Disables the machine state outputting.


Platform size limit options:
  --mmax <nb>                       Limits the number of machines to <nb>.
                                    0 means no limit [default: 0].
  --mmax-workload                   If set, limits the number of machines to
                                    the 'nb_res' field of the input workloads.
                                    If several workloads are used, the maximum
                                    of these fields is kept.
Verbosity options:
  -v, --verbosity <verbosity_level> Sets the Batsim verbosity level. Available
                                    values: quiet, network-only, information,
                                    debug [default: information].
  -q, --quiet                       Shortcut for --verbosity quiet

Workflow options:
  --workflow-jobs-limit <job_limit> Limits the number of possible concurrent
                                    jobs for workflows. 0 means no limit
                                    [default: 0].
  --ignore-beyond-last-workflow     Ignores workload jobs that occur after all
                                    workflows have completed.

Other options:
  --allow-time-sharing              Allows time sharing: One resource may
                                    compute several jobs at the same time.
  --batexec                         If set, the jobs in the workloads are
                                    computed one by one, one after the other,
                                    without scheduler nor Redis.
  --pfs-host <pfs_host>             The name of the host, in <platform_file>,
                                    which will be the parallel filesystem target
                                    as data sink/source for the large-capacity
                                    storage tier [default: pfs_host].
  --hpst-host <hpst_host>           The name of the host, in <platform_file>,
                                    which will be the parallel filesystem target
                                    as data sink/source for the high-performance
                                    storage tier [default: hpst_host].
  -h --help                         Shows this help.
)";

    run_simulation = false,
    run_unit_tests = false;
    return_code = 1;
    map<string, docopt::value> args = docopt::docopt(usage, { argv + 1, argv + argc },
                                                     true, STR(BATSIM_VERSION));

    // Let's do some checks on the arguments!
    bool error = false;
    return_code = 0;

    if (args["--simgrid-version"].asBool())
    {
        int sg_major, sg_minor, sg_patch;
        sg_version(&sg_major, &sg_minor, &sg_patch);

        printf("%d.%d.%d\n", sg_major, sg_minor, sg_patch);
        return;
    }

    if (args["--unittest"].asBool())
    {
        run_unit_tests = true;
        return;
    }

    // Input files
    // ***********
    main_args.platform_filename = args["--platform"].asString();
    printf("platform_filename: %s\n", main_args.platform_filename.c_str());
    if (!file_exists(main_args.platform_filename))
    {
        XBT_ERROR("Platform file '%s' cannot be read.", main_args.platform_filename.c_str());
        error = true;
        return_code |= 0x01;
    }

    // Workloads
    vector<string> workload_files = args["--workload"].asStringList();
    for (const string & workload_file : workload_files)
    {
        if (!file_exists(workload_file))
        {
            XBT_ERROR("Workload file '%s' cannot be read.", workload_file.c_str());
            error = true;
            return_code |= 0x02;
        }
        else
        {
            MainArguments::WorkloadDescription desc;
            desc.filename = absolute_filename(workload_file);
            desc.name = generate_sha1_string(desc.filename);

            XBT_INFO("Workload '%s' corresponds to workload file '%s'.",
                     desc.name.c_str(), desc.filename.c_str());
            main_args.workload_descriptions.push_back(desc);
        }
    }

    // Workflows (without start time)
    vector<string> workflow_files = args["--workflow"].asStringList();
    for (const string & workflow_file : workflow_files)
    {
        if (!file_exists(workflow_file))
        {
            XBT_ERROR("Workflow file '%s' cannot be read.", workflow_file.c_str());
            error = true;
            return_code |= 0x04;
        }
        else
        {
            MainArguments::WorkflowDescription desc;
            desc.filename = absolute_filename(workflow_file);
            desc.name = generate_sha1_string(desc.filename);
            desc.workload_name = desc.name;
            desc.start_time = 0;

            XBT_INFO("Workflow '%s' corresponds to workflow file '%s'.",
                     desc.name.c_str(), desc.filename.c_str());
            main_args.workflow_descriptions.push_back(desc);
        }
    }

    // Workflows (with start time)
    vector<string> cut_workflow_files = args["<cut_workflow_file>"].asStringList();
    vector<string> cut_workflow_times = args["<start_time>"].asStringList();
    if (cut_workflow_files.size() != cut_workflow_times.size())
    {
        XBT_ERROR("--workflow-start parsing results are inconsistent: "
                  "<cut_workflow_file> and <start_time> have different "
                  "sizes (%zu and %zu)", cut_workflow_files.size(),
                  cut_workflow_times.size());
        error = true;
        return_code |= 0x08;
    }
    else
    {
        for (unsigned int i = 0; i < cut_workflow_files.size(); ++i)
        {
            const string & cut_workflow_file = cut_workflow_files[i];
            const string & cut_workflow_time_str = cut_workflow_times[i];
            if (!file_exists(cut_workflow_file))
            {
                XBT_ERROR("Cut workflow file '%s' cannot be read.", cut_workflow_file.c_str());
                error = true;
                return_code |= 0x10;
            }
            else
            {
                MainArguments::WorkflowDescription desc;
                desc.filename = absolute_filename(cut_workflow_file);
                desc.name = generate_sha1_string(desc.filename);
                desc.workload_name = desc.name;
                try
                {
                    desc.start_time = std::stod(cut_workflow_time_str);

                    if (desc.start_time < 0)
                    {
                        XBT_ERROR("<start_time> %g ('%s') should be positive.",
                                  desc.start_time, cut_workflow_time_str.c_str());
                        error = true;
                        return_code |= 0x20;
                    }
                    else
                    {
                        XBT_INFO("Cut workflow '%s' corresponds to workflow file '%s'.",
                                 desc.name.c_str(), desc.filename.c_str());
                        main_args.workflow_descriptions.push_back(desc);
                    }
                }
                catch (const std::exception &)
                {
                    XBT_ERROR("Cannot read the <start_time> '%s' as a double.",
                              cut_workflow_time_str.c_str());
                    error = true;
                    return_code |= 0x40;
                }
            }
        }
    }

    // Common options
    // **************
    main_args.master_host_name = args["--master-host"].asString();
    main_args.energy_used = args["--energy"].asBool();

    // Execution context options
    // *************************
    string config_filename = args["--config-file"].asString();
    if (config_filename != "None")
    {
        XBT_INFO("Reading configuration file '%s'", config_filename.c_str());

        ifstream file(config_filename);
        string file_content((istreambuf_iterator<char>(file)),
                            istreambuf_iterator<char>());
        main_args.config_file.Parse(file_content.c_str());
        xbt_assert(!main_args.config_file.HasParseError(),
                   "Invalid configuration file '%s': could not be parsed.",
                   config_filename.c_str());
    }
    else
    {
        main_args.config_file.Parse(default_configuration.c_str());
        xbt_assert(!main_args.config_file.HasParseError(),
                   "Invalid default configuration file : could not be parsed.");
    }


    main_args.socket_endpoint = args["--socket-endpoint"].asString();
    main_args.redis_hostname = args["--redis-hostname"].asString();
    try
    {
        main_args.redis_port = args["--redis-port"].asLong();
    }
    catch(const std::exception &)
    {
        XBT_ERROR("Cannot read the Redis port '%s' as a long integer.",
                  args["--redis-port"].asString().c_str());
        error = true;
    }
    main_args.redis_prefix = args["--redis-prefix"].asString();


    // Output options
    // **************
    main_args.export_prefix = args["--export"].asString();
    main_args.enable_simgrid_process_tracing = args["--enable-sg-process-tracing"].asBool();
    main_args.enable_schedule_tracing = !args["--disable-schedule-tracing"].asBool();
    main_args.enable_machine_state_tracing = !args["--disable-machine-state-tracing"].asBool();

    // Platform size limit options
    // ***************************
    string m_max_str = args["--mmax"].asString();
    try
    {
        main_args.limit_machines_count = std::stoi(m_max_str);
    }
    catch (const std::exception &)
    {
        XBT_ERROR("Cannot read <M_max> '%s' as an integer.", m_max_str.c_str());
        error = true;
    }

    main_args.limit_machines_count_by_workload = args["--mmax-workload"].asBool();

    // Verbosity options
    // *****************
    try
    {
        main_args.verbosity = verbosity_level_from_string(args["--verbosity"].asString());
        if (args["--quiet"].asBool())
        {
            main_args.verbosity = VerbosityLevel::QUIET;
        }
    }
    catch (const std::exception &)
    {
        XBT_ERROR("Invalid <verbosity_level> '%s'.", args["--verbosity"].asString().c_str());
        error = true;
    }

    // Workflow options
    // ****************
    string workflow_jobs_limit = args["--workflow-jobs-limit"].asString();
    try
    {
        main_args.workflow_nb_concurrent_jobs_limit = std::stoi(workflow_jobs_limit);
        if (main_args.workflow_nb_concurrent_jobs_limit < 0)
        {
            XBT_ERROR("The <workflow_limit> %d ('%s') must be positive.", main_args.workflow_nb_concurrent_jobs_limit,
                      workflow_jobs_limit.c_str());
            error = true;
        }
    }
    catch (const std::exception &)
    {
        XBT_ERROR("Cannot read the <job_limit> '%s' as an integer.", workflow_jobs_limit.c_str());
        error = true;
    }

    main_args.terminate_with_last_workflow = args["--ignore-beyond-last-workflow"].asBool();

    // Other options
    // *************
    main_args.allow_time_sharing = args["--allow-time-sharing"].asBool();
    if (args["--batexec"].asBool())
    {
        main_args.program_type = ProgramType::BATEXEC;
    }
    else
    {
        main_args.program_type = ProgramType::BATSIM;
    }
    main_args.pfs_host_name = args["--pfs-host"].asString();
    main_args.hpst_host_name = args["--hpst-host"].asString();

    run_simulation = !error;
}

void configure_batsim_logging_output(const MainArguments & main_args)
{
    vector<string> log_categories_to_set = {"workload", "job_submitter", "redis", "jobs", "machines", "pstate",
                                            "workflow", "jobs_execution", "server", "export", "profiles", "machine_range",
                                            "network", "ipp"};
    string log_threshold_to_set = "critical";

    if (main_args.verbosity == VerbosityLevel::QUIET || main_args.verbosity == VerbosityLevel::NETWORK_ONLY)
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

    // In network-only, we add a rule to display the network info
    if (main_args.verbosity == VerbosityLevel::NETWORK_ONLY)
    {
        xbt_log_control_set("network.thresh:info");
    }

    // Batsim is always set to info, to allow to trace Batsim's input easily
    xbt_log_control_set("batsim.thresh:info");

    // Simgrid-related log control
    xbt_log_control_set("surf_energy.thresh:critical");
}

void initialize_msg(const MainArguments & main_args, int argc, char * argv[])
{
    // Must be initialized before MSG_init
    if (main_args.energy_used)
    {
        sg_energy_plugin_init();
    }

    MSG_init(&argc, argv);

    // Setting SimGrid configuration if the SimGrid process tracing is enabled
    if (main_args.enable_simgrid_process_tracing)
    {
        string sg_trace_filename = main_args.export_prefix + "_sg_processes.trace";

        MSG_config("tracing", "1");
        MSG_config("tracing/msg/process", "1");
        MSG_config("tracing/filename", sg_trace_filename.c_str());
    }
}

void load_workloads_and_workflows(const MainArguments & main_args, BatsimContext * context, int & max_nb_machines_to_use)
{
    int max_nb_machines_in_workloads = -1;

    // Let's create the workloads
    for (const MainArguments::WorkloadDescription & desc : main_args.workload_descriptions)
    {
        Workload * workload = new Workload(desc.name, desc.filename);

        int nb_machines_in_workload = -1;
        workload->load_from_json(desc.filename, nb_machines_in_workload);
        max_nb_machines_in_workloads = std::max(max_nb_machines_in_workloads, nb_machines_in_workload);

        context->workloads.insert_workload(desc.name, workload);
    }

    // Let's create the workflows
    for (const MainArguments::WorkflowDescription & desc : main_args.workflow_descriptions)
    {
        Workload * workload = new Workload(desc.workload_name, desc.filename); // Is creating the Workload now necessary? Workloads::add_job_if_not_exists may be enough
        workload->jobs = new Jobs;
        workload->profiles = new Profiles;
        context->workloads.insert_workload(desc.workload_name, workload);

        Workflow * workflow = new Workflow(desc.name);
        workflow->start_time = desc.start_time;
        workflow->load_from_xml(desc.filename);
        context->workflows.insert_workflow(desc.name, workflow);
    }

    // Let's compute how the number of machines to use should be limited
    max_nb_machines_to_use = 0;
    if ((main_args.limit_machines_count_by_workload) && (main_args.limit_machines_count > 0))
    {
        max_nb_machines_to_use = std::min(main_args.limit_machines_count, max_nb_machines_in_workloads);
    }
    else if (main_args.limit_machines_count_by_workload)
    {
        max_nb_machines_to_use = max_nb_machines_in_workloads;
    }
    else if (main_args.limit_machines_count > 0)
    {
        max_nb_machines_to_use = main_args.limit_machines_count;
    }

    if (max_nb_machines_to_use != 0)
    {
        XBT_INFO("The maximum number of machines to use is %d.", max_nb_machines_to_use);
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
        XBT_DEBUG("Creating a workload_submitter process...");
        JobSubmitterProcessArguments * submitter_args = new JobSubmitterProcessArguments;
        submitter_args->context = context;
        submitter_args->workload_name = desc.name;

        string submitter_instance_name = "workload_submitter_" + desc.name;
        if (!is_batexec)
        {
            MSG_process_create(submitter_instance_name.c_str(), static_job_submitter_process,
                               (void*) submitter_args, master_machine->host);
        }
        else
        {
            MSG_process_create(submitter_instance_name.c_str(), batexec_job_launcher_process,
                               (void*) submitter_args, master_machine->host);
        }
        XBT_INFO("The process '%s' has been created.", submitter_instance_name.c_str());
    }

    // Let's run a workflow_submitter process for each workflow
    for (const MainArguments::WorkflowDescription & desc : main_args.workflow_descriptions)
    {
        XBT_DEBUG("Creating a workflow_submitter process...");

        WorkflowSubmitterProcessArguments * submitter_args = new WorkflowSubmitterProcessArguments;
        submitter_args->context = context;
        submitter_args->workflow_name = desc.name;

        string submitter_instance_name = "workflow_submitter_" + desc.name;
        MSG_process_create(submitter_instance_name.c_str(), workflow_submitter_process, (void*)submitter_args, master_machine->host);
        XBT_INFO("The process '%s' has been created.", submitter_instance_name.c_str());
    }

    if (!is_batexec)
    {
        XBT_DEBUG("Creating the 'server' process...");
        ServerProcessArguments * server_args = new ServerProcessArguments;
        server_args->context = context;
        MSG_process_create("server", server_process, (void*)server_args, master_machine->host);
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
    bool run_unittests = false;

    parse_main_args(argc, argv, main_args, return_code, run_simulation, run_unittests);

    if (run_unittests)
    {
        MSG_init(&argc, argv);
        test_entry_point();
    }

    if (!run_simulation)
    {
        return return_code;
    }

    // Let's configure how Batsim should be logged
    configure_batsim_logging_output(main_args);

    // Let's initialize SimGrid
    initialize_msg(main_args, argc, argv);

    // Let's create the BatsimContext, which stores information about the current instance
    BatsimContext context;
    set_configuration(&context, main_args);

    context.batsim_version = STR(BATSIM_VERSION);
    XBT_INFO("Batsim version: %s", context.batsim_version.c_str());

    // Let's load the workloads and workflows
    int max_nb_machines_to_use = -1;
    load_workloads_and_workflows(main_args, &context, max_nb_machines_to_use);

    // Let's choose which SimGrid computing model should be used
    XBT_INFO("Checking whether SMPI is used or not...");
    context.smpi_used = context.workloads.contains_smpi_job(); // todo: SMPI workflows
    if (!context.smpi_used)
    {
        XBT_INFO("SMPI will NOT be used.");
        MSG_config("host/model", "ptask_L07");
    }
    else
    {
        XBT_INFO("SMPI will be used.");
        context.workloads.register_smpi_applications(); // todo: SMPI workflows
        SMPI_init();
    }

    // Let's create the machines
    create_machines(main_args, &context, max_nb_machines_to_use);

    // Let's prepare Batsim's outputs
    XBT_INFO("Batsim's export prefix is '%s'.", context.export_prefix.c_str());
    prepare_batsim_outputs(&context);

    if (main_args.program_type == ProgramType::BATSIM)
    {
        if (context.redis_enabled)
        {
            // Let's prepare Redis's connection
            context.storage.set_instance_key_prefix(main_args.redis_prefix);
            context.storage.connect_to_server(main_args.redis_hostname, main_args.redis_port);

            // Let's store some metadata about the current instance in the data storage
            context.storage.set("nb_res", std::to_string(context.machines.nb_machines()));
        }

        // Let's create the socket
        context.zmq_socket = new zmq::socket_t(context.zmq_context, ZMQ_REQ);
        context.zmq_socket->connect(main_args.socket_endpoint);

        // Let's create the protocol reader and writer
        context.proto_reader = new JsonProtocolReader(&context);
        context.proto_writer = new JsonProtocolWriter(&context);

        // Let's execute the initial processes
        start_initial_simulation_processes(main_args, &context);
    }
    else if (main_args.program_type == ProgramType::BATEXEC)
    {
        // Let's execute the initial processes
        start_initial_simulation_processes(main_args, &context, true);
    }

    // Simulation main loop, handled by MSG
    msg_error_t res = MSG_main();

    delete context.zmq_socket;
    context.zmq_socket = nullptr;

    delete context.proto_reader;
    context.proto_reader = nullptr;

    delete context.proto_writer;
    context.proto_writer = nullptr;

    // If SMPI had been used, it should be finalized
    if (context.smpi_used)
    {
        SMPI_finalize();
    }

    // Let's finalize Batsim's outputs
    finalize_batsim_outputs(&context);

    if (res == MSG_OK)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void set_configuration(BatsimContext *context,
                       MainArguments & main_args)
{
    using namespace rapidjson;

    // ********************************************************
    // Let's load default values from the default configuration
    // ********************************************************
    Document default_config_doc;
    default_config_doc.Parse(default_configuration.c_str());
    xbt_assert(!default_config_doc.HasParseError(),
               "Invalid default configuration file : could not be parsed.");

    bool redis_enabled = default_config_doc["redis"]["enabled"].GetBool();
    string redis_hostname = default_config_doc["redis"]["hostname"].GetString();
    int redis_port = default_config_doc["redis"]["port"].GetInt();
    string redis_prefix = default_config_doc["redis"]["prefix"].GetString();

    bool submission_forward_profiles = default_config_doc["job_submission"]["forward_profiles"].GetBool();

    bool submission_sched_enabled = default_config_doc["job_submission"]["from_scheduler"]["enabled"].GetBool();
    bool submission_sched_ack = default_config_doc["job_submission"]["from_scheduler"]["acknowledge"].GetBool();

    // **********************************
    // Let's parse the configuration file
    // **********************************
    const Value & main_object = main_args.config_file;
    xbt_assert(main_object.IsObject(), "Invalid JSON configuration: not an object.");

    if(main_object.HasMember("redis"))
    {
        const Value & redis_object = main_object["redis"];
        xbt_assert(redis_object.IsObject(), "Invalid JSON configuration: ['redis'] should be an object.");

        if (redis_object.HasMember("enabled"))
        {
            const Value & redis_enabled_value = redis_object["enabled"];
            xbt_assert(redis_enabled_value.IsBool(), "Invalid JSON configuration: ['redis']['enabled'] should be a boolean.");
            redis_enabled = redis_enabled_value.GetBool();
        }

        if (redis_object.HasMember("hostname"))
        {
            const Value & redis_hostname_value = redis_object["hostname"];
            xbt_assert(redis_hostname_value.IsString(), "Invalid JSON configuration: ['redis']['hostname'] should be a string.");
            redis_hostname = redis_hostname_value.GetString();
        }

        if (redis_object.HasMember("port"))
        {
            const Value & redis_port_value = redis_object["port"];
            xbt_assert(redis_port_value.IsInt(), "Invalid JSON configuration: ['redis']['port'] should be an integer.");
            redis_port = redis_port_value.GetInt();
        }

        if (redis_object.HasMember("prefix"))
        {
            const Value & redis_prefix_value = redis_object["prefix"];
            xbt_assert(redis_prefix_value.IsString(), "Invalid JSON configuration: ['redis']['prefix'] should be a string.");
            redis_prefix = redis_prefix_value.GetString();
        }
    }
    if (main_object.HasMember("job_submission"))
    {
        const Value & job_submission_object = main_object["job_submission"];
        xbt_assert(job_submission_object.IsObject(), "Invalid JSON configuration: ['job_submission'] should be an object.");

        if (job_submission_object.HasMember("forward_profiles"))
        {
            const Value & forward_profiles_value = job_submission_object["forward_profiles"];
            xbt_assert(forward_profiles_value.IsBool(), "Invalid JSON configuration: ['job_submission']['forward_profiles'] should be a boolean.");
            submission_forward_profiles = forward_profiles_value.GetBool();
        }

        if (job_submission_object.HasMember("from_scheduler"))
        {
            const Value & from_sched_object = job_submission_object["from_scheduler"];
            xbt_assert(from_sched_object.IsObject(), "Invalid JSON configuration: ['job_submission']['from_scheduler'] should be an object.");

            if (from_sched_object.HasMember("enabled"))
            {
                const Value & submission_sched_enabled_value = from_sched_object["enabled"];
                xbt_assert(submission_sched_enabled_value.IsBool(), "Invalid JSON configuration: ['job_submission']['enabled'] should be a boolean.");
                submission_sched_enabled = submission_sched_enabled_value.GetBool();
            }

            if (from_sched_object.HasMember("acknowledge"))
            {
                const Value & submission_sched_ack_value = from_sched_object["acknowledge"];
                xbt_assert(submission_sched_ack_value.IsBool(), "Invalid JSON configuration: ['job_submission']['acknowledge'] should be a boolean.");
                submission_sched_ack = submission_sched_ack_value.GetBool();
            }
        }
    }

    // *****************************************************************
    // Let's override configuration values from main arguments if needed
    // *****************************************************************
    if (main_args.redis_hostname != "None")
    {
        redis_hostname = main_args.redis_hostname;
    }
    if (main_args.redis_port != -1)
    {
        redis_port = main_args.redis_port;
    }
    if (main_args.redis_prefix != "None")
    {
        redis_prefix = main_args.redis_prefix;
    }

    // *************************************
    // Let's update the BatsimContext values
    // *************************************
    context->redis_enabled = redis_enabled;
    context->submission_forward_profiles = submission_forward_profiles;
    context->submission_sched_enabled = submission_sched_enabled;
    context->submission_sched_ack = submission_sched_ack;

    context->platform_filename = main_args.platform_filename;
    context->export_prefix = main_args.export_prefix;
    context->workflow_nb_concurrent_jobs_limit = main_args.workflow_nb_concurrent_jobs_limit;
    context->energy_used = main_args.energy_used;
    context->allow_time_sharing = main_args.allow_time_sharing;
    context->trace_schedule = main_args.enable_schedule_tracing;
    context->trace_machine_states = main_args.enable_machine_state_tracing;
    context->simulation_start_time = chrono::high_resolution_clock::now();
    context->terminate_with_last_workflow = main_args.terminate_with_last_workflow;

    // *************************************
    // Let's update the MainArguments values
    // *************************************
    main_args.redis_hostname = redis_hostname;
    main_args.redis_port = redis_port;
    main_args.redis_prefix = redis_prefix;

    // *******************************************************************************
    // Let's write the output config file (the one that will be sent to the scheduler)
    // *******************************************************************************
    auto & alloc = context->config_file.GetAllocator();

    // Let's retrieve all data specified in the input config file (to let the user give custom info to the scheduler)
    context->config_file.CopyFrom(main_args.config_file, alloc);

    // Let's make sure all used data is written too.
    // redis
    auto mit_redis = context->config_file.FindMember("redis");
    if (mit_redis == context->config_file.MemberEnd())
    {
        context->config_file.AddMember("redis", Value().SetObject(), alloc);
        mit_redis = context->config_file.FindMember("redis");
    }

    // redis->enabled
    if (mit_redis->value.FindMember("enabled") == mit_redis->value.MemberEnd())
    {
        mit_redis->value.AddMember("enabled", Value().SetBool(redis_enabled), alloc);
    }

    // redis->hostname
    Value::MemberIterator mit_redis_hostname = mit_redis->value.FindMember("hostname");
    if (mit_redis_hostname == mit_redis->value.MemberEnd())
    {
        mit_redis->value.AddMember("hostname", Value().SetString(redis_hostname.c_str(), alloc), alloc);
    }
    else
    {
        mit_redis_hostname->value.SetString(redis_hostname.c_str(), alloc);
    }

    // redis->port
    Value::MemberIterator mit_redis_port = mit_redis->value.FindMember("port");
    if (mit_redis_port == mit_redis->value.MemberEnd())
    {
        mit_redis->value.AddMember("port", Value().SetInt(redis_port), alloc);
    }
    else
    {
        mit_redis_port->value.SetInt(redis_port);
    }

    // redis->prefix
    Value::MemberIterator mit_redis_prefix = mit_redis->value.FindMember("prefix");
    if (mit_redis_prefix == mit_redis->value.MemberEnd())
    {
        mit_redis->value.AddMember("prefix", Value().SetString(redis_prefix.c_str(), alloc), alloc);
    }
    else
    {
        mit_redis_prefix->value.SetString(redis_prefix.c_str(), alloc);
    }


    // job_submission
    auto mit_job_submission = context->config_file.FindMember("job_submission");
    if (mit_job_submission == context->config_file.MemberEnd())
    {
        context->config_file.AddMember("job_submission", Value().SetObject(), alloc);
        mit_job_submission = context->config_file.FindMember("job_submission");
    }

    // job_submission->forward_profiles
    if (mit_job_submission->value.FindMember("forward_profiles") == mit_job_submission->value.MemberEnd())
    {
        mit_job_submission->value.AddMember("forward_profiles", Value().SetBool(submission_forward_profiles), alloc);
    }

    // job_submission->from_scheduler
    auto mit_job_submission_from_sched = mit_job_submission->value.FindMember("from_scheduler");
    if (mit_job_submission_from_sched == mit_job_submission->value.MemberEnd())
    {
        mit_job_submission->value.AddMember("from_scheduler", Value().SetObject(), alloc);
        mit_job_submission_from_sched = mit_job_submission->value.FindMember("from_scheduler");
    }
    Value & from_sched_value = mit_job_submission_from_sched->value;

    // job_submission_from_scheduler->enabled
    if (from_sched_value.FindMember("enabled") == from_sched_value.MemberEnd())
    {
        from_sched_value.AddMember("enabled", Value().SetBool(submission_sched_enabled), alloc);
    }

    // job_submission_from_scheduler->acknowledge
    if (from_sched_value.FindMember("acknowledge") == from_sched_value.MemberEnd())
    {
        from_sched_value.AddMember("acknowledge", Value().SetBool(submission_sched_ack), alloc);
    }
}

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
#include <functional>

#include <simgrid/s4u.hpp>
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
#include "event_submitter.hpp"
#include "events.hpp"
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
 * @brief Computes the absolute filename of a given file
 * @param[in] filename The name of the file (not necessarily existing).
 * @return The absolute filename corresponding to the given filename
 */
std::string absolute_filename(const std::string & filename)
{
    xbt_assert(filename.length() > 0);

    // Let's assume filenames starting by "/" are absolute.
    if (filename[0] == '/')
    {
        return filename;
    }

    char cwd_buf[PATH_MAX];
    char * getcwd_ret = getcwd(cwd_buf, PATH_MAX);
    xbt_assert(getcwd_ret == cwd_buf, "getcwd failed");

    return string(getcwd_ret) + '/' + filename;
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

    // TODO: change hosts format in roles to support intervals
    // The <hosts> should be formated as follow:
    // hostname[intervals],hostname[intervals],...
    // Where `intervals` is a comma separated list of simple integer
    // or closed interval separated with a '-'.
    // Example: -r host[1-5,8]:role1,role2 -r toto,tata:myrole
 

    static const char usage[] =
R"(A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.

Usage:
  batsim -p <platform_file> [-w <workload_file>...]
                            [-W <workflow_file>...]
                            [--WS (<cut_workflow_file> <start_time>)...]
                            [--sg-cfg <opt_name:opt_value>...]
                            [--sg-log <log_option>...]
                            [-r <hosts_roles_map>...]
                            [--events <events_file>...]
                            [options]
  batsim --help
  batsim --version
  batsim --simgrid-version
  batsim --unittest

Input options:
  -p, --platform <platform_file>     The SimGrid platform to simulate.
  -w, --workload <workload_file>     The workload JSON files to simulate.
  -W, --workflow <workflow_file>     The workflow XML files to simulate.
  --WS, --workflow-start (<cut_workflow_file> <start_time>)  The workflow XML
                                     files to simulate, with the time at which
                                     they should be started.
  --events <events_file>             The files containing events to simulate.

Most common options:
  -m, --master-host <name>           The name of the host in <platform_file>
                                     which will be used as the RJMS management
                                     host (thus NOT used to compute jobs)
                                     [default: master_host].
  -r, --add-role-to-hosts <hosts_role_map>  Add a `role` property to the specify host(s).
                                     The <hosts-roles-map> is formated as <hosts>:<role>
                                     The <hosts> should be formated as follow:
                                     hostname1,hostname2,..
                                     Supported roles are: master, storage, compute_node
                                     By default, no role means 'compute_node'
                                     Example: -r host8:master -r host1,host2:storage
  -E, --energy                       Enables the SimGrid energy plugin and
                                     outputs energy-related files.

Execution context options:
  -s, --socket-endpoint <endpoint>   The Decision process socket endpoint
                                     Decision process [default: tcp://localhost:28000].
  --enable-redis                     Enables Redis to communicate with the scheduler.
                                     Other redis options are ignored if this option is not set.
                                     Please refer to Batsim's documentation for more information.
  --redis-hostname <redis_host>      The Redis server hostname. Ignored if --enable-redis is not set.
                                     [default: 127.0.0.1]
  --redis-port <redis_port>          The Redis server port. Ignored if --enable-redis is not set.
                                     [default: 6379]
  --redis-prefix <prefix>            The Redis prefix. Ignored if --enable-redis is not set.
                                     [default: default]

Output options:
  -e, --export <prefix>              The export filename prefix used to generate
                                     simulation output [default: out].
  --disable-schedule-tracing         Disables the Pajé schedule outputting.
  --disable-machine-state-tracing    Disables the machine state outputting.

Platform size limit options:
  --mmax <nb>                        Limits the number of machines to <nb>.
                                     0 means no limit [default: 0].
  --mmax-workload                    If set, limits the number of machines to
                                     the 'nb_res' field of the input workloads.
                                     If several workloads are used, the maximum
                                     of these fields is kept.
Job-related options:
  --forward-profiles-on-submission   Attaches the job profile to the job information
                                     when the scheduler is notified about a job submission.
                                     [default: false]
  --enable-dynamic-jobs              Enables dynamic registration of jobs and profiles from the scheduler.
                                     Please refer to Batsim's documentation for more information.
                                     [default: false]
  --acknowledge-dynamic-jobs         Makes Batsim send a JOB_SUBMITTED back to the scheduler when
                                     Batsim receives a REGISTER_JOB.
                                     [default: true]

Verbosity options:
  -v, --verbosity <verbosity_level>  Sets the Batsim verbosity level. Available
                                     values: quiet, network-only, information,
                                     debug [default: information].
  -q, --quiet                        Shortcut for --verbosity quiet

Workflow options:
  --workflow-jobs-limit <job_limit>  Limits the number of possible concurrent
                                     jobs for workflows. 0 means no limit
                                     [default: 0].
  --ignore-beyond-last-workflow      Ignores workload jobs that occur after all
                                     workflows have completed.

Other options:
  --dump-execution-context           Does not run the actual simulation but dumps the execution
                                     context on stdout (formatted as a JSON object).
  --enable-compute-sharing           Enables compute resource sharing:
                                     One compute resource may be used by several jobs at the same time.
  --disable-storage-sharing          Disables storage resource sharing:
                                     One storage resource may be used by several jobs at the same time.
  --no-sched                         If set, the jobs in the workloads are
                                     computed one by one, one after the other,
                                     without scheduler nor Redis.
  --sg-cfg <opt_name:opt_value>      Forwards a given option_name:option_value to SimGrid.
                                     Refer to SimGrid configuring documentation for more information.
  --sg-log <log_option>              Forwards a given logging option to SimGrid.
                                     Refer to SimGrid simulation logging documentation for more information.
  -h, --help                         Shows this help.
)";

    run_simulation = false;
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
        sg_version_get(&sg_major, &sg_minor, &sg_patch);

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

    // EventLists
    vector<string> events_files = args["--events"].asStringList();
    for (const string & events_file : events_files)
    {
        if (!file_exists(events_file))
        {
            XBT_ERROR("Events file '%s' cannot be read.", events_file.c_str());
            error = true;
            return_code |= 0x02;
        }
        else
        {
            MainArguments::EventListDescription desc;
            desc.filename = absolute_filename(events_file);
            desc.name = generate_sha1_string(desc.filename);

            XBT_INFO("Event list '%s' corresponds to events file '%s'.",
                     desc.name.c_str(), desc.filename.c_str());
            main_args.eventList_descriptions.push_back(desc);
        }
    }

    // Common options
    // **************
    main_args.hosts_roles_map = map<string, string>();

    main_args.master_host_name = args["--master-host"].asString();
    main_args.hosts_roles_map[main_args.master_host_name] = "master";

    main_args.energy_used = args["--energy"].asBool();


    // get roles mapping
    vector<string> hosts_roles_maps = args["--add-role-to-hosts"].asStringList();
    for (unsigned int i = 0; i < hosts_roles_maps.size(); ++i)
    {
        vector<string> parsed;
        boost::split(parsed, hosts_roles_maps[i], boost::is_any_of(":"));

        xbt_assert(parsed.size() == 2, "The roles host mapping should only contain one ':' character");
        string hosts = parsed[0];
        string roles = parsed[1];
        vector<string> host_list;

        boost::split(host_list, hosts, boost::is_any_of(","));

        for (auto & host: host_list)
        {
            main_args.hosts_roles_map[host] = roles;
        }
    }

    main_args.socket_endpoint = args["--socket-endpoint"].asString();
    main_args.redis_enabled = args["--enable-redis"].asBool();
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
    main_args.enable_schedule_tracing = !args["--disable-schedule-tracing"].asBool();
    main_args.enable_machine_state_tracing = !args["--disable-machine-state-tracing"].asBool();

    // Job-related options
    // *******************
    main_args.forward_profiles_on_submission = args["--forward-profiles-on-submission"].asBool();
    main_args.dynamic_registration_enabled = args["--enable-dynamic-jobs"].asBool();
    main_args.ack_dynamic_registration = args["--acknowledge-dynamic-jobs"].asBool();

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
    main_args.dump_execution_context = args["--dump-execution-context"].asBool();
    main_args.allow_compute_sharing = args["--enable-compute-sharing"].asBool();
    main_args.allow_storage_sharing = !(args["--disable-storage-sharing"].asBool());
    if (args["--no-sched"].asBool())
    {
        main_args.program_type = ProgramType::BATEXEC;
    }
    else
    {
        main_args.program_type = ProgramType::BATSIM;
    }

    main_args.simgrid_config = args["--sg-cfg"].asStringList();
    main_args.simgrid_logging = args["--sg-log"].asStringList();

    run_simulation = !error;
}

void configure_batsim_logging_output(const MainArguments & main_args)
{
    vector<string> log_categories_to_set = {"workload", "job_submitter", "redis", "jobs", "machines", "pstate",
                                            "workflow", "jobs_execution", "server", "export", "profiles", "machine_range",
                                            "network", "ipp", "task_execution"};
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

void load_workloads_and_workflows(const MainArguments & main_args, BatsimContext * context, int & max_nb_machines_to_use)
{
    int max_nb_machines_in_workloads = -1;

    // Let's create the workloads
    for (const MainArguments::WorkloadDescription & desc : main_args.workload_descriptions)
    {
        Workload * workload = Workload::new_static_workload(desc.name, desc.filename);

        int nb_machines_in_workload = -1;
        workload->load_from_json(desc.filename, nb_machines_in_workload);
        max_nb_machines_in_workloads = std::max(max_nb_machines_in_workloads, nb_machines_in_workload);

        context->workloads.insert_workload(desc.name, workload);
    }

    // Let's create the workflows
    for (const MainArguments::WorkflowDescription & desc : main_args.workflow_descriptions)
    {
        Workload * workload = Workload::new_static_workload(desc.workload_name, desc.filename);
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

void load_eventLists(const MainArguments & main_args, BatsimContext * context)
{
    for (const MainArguments::EventListDescription & desc : main_args.eventList_descriptions)
    {
        EventList * events = EventList::new_event_list(desc.name);
        events->load_from_json(desc.filename);
        context->eventListsMap[desc.name] = events;
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
    bool run_unittests = false;

    parse_main_args(argc, argv, main_args, return_code, run_simulation, run_unittests);

    if (run_unittests)
    {
        MSG_init(&argc, argv);
        test_entry_point();
    }
    else if (main_args.dump_execution_context)
    {
        using namespace rapidjson;
        Document object;
        auto & alloc = object.GetAllocator();
        object.SetObject();

        // Generate the content to dump
        object.AddMember("socket_endpoint", Value().SetString(main_args.socket_endpoint.c_str(), alloc), alloc);
        object.AddMember("redis_enabled", Value().SetBool(main_args.redis_enabled), alloc);
        object.AddMember("redis_hostname", Value().SetString(main_args.redis_hostname.c_str(), alloc), alloc);
        object.AddMember("redis_port", Value().SetInt(main_args.redis_port), alloc);
        object.AddMember("redis_prefix", Value().SetString(main_args.redis_prefix.c_str(), alloc), alloc);

        object.AddMember("export_prefix", Value().SetString(main_args.export_prefix.c_str(), alloc), alloc);

        object.AddMember("external_scheduler", Value().SetBool(main_args.program_type == ProgramType::BATSIM), alloc);

        // Dump the object to a string
        StringBuffer buffer;
        rapidjson::Writer<StringBuffer> writer(buffer);
        object.Accept(writer);

        // Print the string then terminate
        printf("%s\n", buffer.GetString());
        return 0;
    }

    if (!run_simulation)
    {
        return return_code;
    }

    // Let's configure how Batsim should be logged
    configure_batsim_logging_output(main_args);

    // Initialize the energy plugin before creating the engine
    if (main_args.energy_used)
    {
        sg_host_energy_plugin_init();
    }

    // Instantiate SimGrid
    MSG_init(&argc, argv); // Required for SMPI as I write these lines
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

    // Let's create the machines
    create_machines(main_args, &context, max_nb_machines_to_use);

    // Let's prepare Batsim's outputs
    XBT_INFO("Batsim's export prefix is '%s'.", context.export_prefix.c_str());
    prepare_batsim_outputs(&context);

    if (main_args.program_type == ProgramType::BATSIM)
    {
        if (context.redis_enabled)
        {
            // Let's prepare Redis' connection
            context.storage.set_instance_key_prefix(main_args.redis_prefix);
            context.storage.connect_to_server(main_args.redis_hostname, main_args.redis_port);

            // Let's store some metadata about the current instance in the data storage
            context.storage.set("nb_res", std::to_string(context.machines.nb_machines()));
        }

        // Let's create the socket
        context.zmq_context = zmq_ctx_new();
        xbt_assert(context.zmq_context != nullptr, "Cannot create ZMQ context");
        context.zmq_socket = zmq_socket(context.zmq_context, ZMQ_REQ);
        xbt_assert(context.zmq_socket != nullptr, "Cannot create ZMQ REQ socket (errno=%s)", strerror(errno));
        int err = zmq_connect(context.zmq_socket, main_args.socket_endpoint.c_str());
        xbt_assert(err == 0, "Cannot connect ZMQ socket to '%s' (errno=%s)", main_args.socket_endpoint.c_str(), strerror(errno));

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
    engine.run();

    zmq_close(context.zmq_socket);
    context.zmq_socket = nullptr;

    zmq_ctx_destroy(context.zmq_context);
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
    context->redis_enabled = main_args.redis_enabled;
    context->submission_forward_profiles = main_args.forward_profiles_on_submission;
    context->registration_sched_enabled = main_args.dynamic_registration_enabled;
    context->registration_sched_ack = main_args.ack_dynamic_registration;

    context->platform_filename = main_args.platform_filename;
    context->export_prefix = main_args.export_prefix;
    context->workflow_nb_concurrent_jobs_limit = main_args.workflow_nb_concurrent_jobs_limit;
    context->energy_used = main_args.energy_used;
    context->allow_compute_sharing = main_args.allow_compute_sharing;
    context->allow_storage_sharing = main_args.allow_storage_sharing;
    context->trace_schedule = main_args.enable_schedule_tracing;
    context->trace_machine_states = main_args.enable_machine_state_tracing;
    context->simulation_start_time = chrono::high_resolution_clock::now();
    context->terminate_with_last_workflow = main_args.terminate_with_last_workflow;

    // **************************************************************************************
    // Let's write the json object holding configuration information to send to the scheduler
    // **************************************************************************************
    context->config_json.SetObject();
    auto & alloc = context->config_json.GetAllocator();

    // redis
    context->config_json.AddMember("redis-enabled", Value().SetBool(main_args.redis_enabled), alloc);
    context->config_json.AddMember("redis-hostname", Value().SetString(main_args.redis_hostname.c_str(), alloc), alloc);
    context->config_json.AddMember("redis-port", Value().SetInt(main_args.redis_port), alloc);
    context->config_json.AddMember("redis-prefix", Value().SetString(main_args.redis_prefix.c_str(), alloc), alloc);

    // job_submission
    context->config_json.AddMember("profiles-forwarded-on-submission", Value().SetBool(main_args.forward_profiles_on_submission), alloc);
    context->config_json.AddMember("dynamic-jobs-enabled", Value().SetBool(main_args.dynamic_registration_enabled), alloc);
    context->config_json.AddMember("dynamic-jobs-acknowledged", Value().SetBool(main_args.ack_dynamic_registration), alloc);
}

#include "cli.hpp"

#include "docopt/docopt.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include <simgrid/s4u.hpp>
#include <simgrid/version.h>

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(cli, "cli"); //!< Logging

/**
 * @brief Checks whether a file exists
 * @param[in] filename The file whose existence is checked<
 * @return true if and only if filename exists
 */
bool file_exists(const std::string & filename)
{
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

/**
 * @brief Computes the absolute filename of a given file
 * @param[in] filename The name of the file (not necessarily existing).
 * @return The absolute filename corresponding to the given filename
 */
std::string absolute_filename(const std::string & filename)
{
    xbt_assert(filename.length() > 0, "filename '%s' is not a filename...", filename.c_str());

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

void parse_main_args(int argc, char * argv[], MainArguments & main_args, int & return_code, bool & run_simulation)
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
  batsim -p <platform_file> [-s <endpoint>...]
                            [-l <path>...]
                            [-w <workload_file>...]
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

Input options:
  -p, --platform <platform_file>     The SimGrid platform to simulate.
  -w, --workload <workload_file>     The workload JSON files to simulate.
  -W, --workflow <workflow_file>     The workflow XML files to simulate.
  --WS, --workflow-start (<cut_workflow_file> <start_time>)  The workflow XML
                                     files to simulate, with the time at which
                                     they should be started.
  --events <events_file>             The files containing external events to simulate.

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
  -s, --edc-socket <endpoint>        Add an external decision component as a process
                                     called through RPC via a ZMQ socket.
                                     Example value: tcp://localhost:28000.
  -l, --edc-library <path>           Add an external decision component as a library
                                     called through a C API.
  -j, --enable-edc-json-format       Enable the use of JSON instead of flatbuffers's binary format
                                     for communications with external decision components.
                                     [default: false]

Output options:
  -e, --export <prefix>              The export filename prefix used to generate
                                     simulation output [default: out/].
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
                                     [default: false]
  --enable-profile-reuse             Enable dynamic jobs to reuse profiles of other jobs.
                                     Without this options, such profiles would be
                                     garbage collected.
                                     The option --enable-dynamic-jobs must be set for this option to work.
                                     [default: false]

Verbosity options:
  -v, --verbosity <verbosity_level>  Sets the Batsim verbosity level. Available
                                     values: quiet, information, debug
                                     [default: information].
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
                                     without scheduler.
  --sg-cfg <opt_name:opt_value>      Forwards a given option_name:option_value to SimGrid.
                                     Refer to SimGrid configuring documentation for more information.
  --sg-log <log_option>              Forwards a given logging option to SimGrid.
                                     Refer to SimGrid simulation logging documentation for more information.
  --forward-unknown-events           Enables the forwarding to the scheduler of external events that
                                     are unknown to Batsim. Ignored if there were no event inputs with --events.
                                     [default: false]
  --edc-library-load-method <method> The method used to load external decision components as libraries.
                                     Available values: dlmopen, dlopen. [default: dlmopen]
  -h, --help                         Shows this help.
)";

    run_simulation = false;
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
    for (size_t i = 0; i < workload_files.size(); i++)
    {
        const string & workload_file = workload_files[i];
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
            desc.name = string("w") + to_string(i);

            XBT_INFO("Workload '%s' corresponds to workload file '%s'.",
                     desc.name.c_str(), desc.filename.c_str());
            main_args.workload_descriptions.push_back(desc);
        }
    }

    // Workflows (without start time)
    vector<string> workflow_files = args["--workflow"].asStringList();
    for (size_t i = 0; i < workflow_files.size(); i++)
    {
        const string & workflow_file = workflow_files[i];
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
            desc.name = string("wf") + to_string(i);
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
                desc.name = string("wfc") + to_string(i);
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
    for (size_t i = 0; i < events_files.size(); i++)
    {
        const string & events_file = events_files[i];
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
            desc.name = string("we") + to_string(i);

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

    // External decision components
    // ****************************
    vector<string> edc_socket_endpoints = args["--edc-socket"].asStringList();
    vector<string> edc_library_paths = args["--edc-library"].asStringList();
    const auto nb_edc = edc_socket_endpoints.size() + edc_library_paths.size();
    xbt_assert(nb_edc <= 1, "Simulation with several external decision components is not supported for now, aborting.");

    if (nb_edc == 0)
    {
        XBT_ERROR("At least one external decision component should be set. This can be done via --edc-socket or --edc-library command-line options.");
        error = true;
        return_code |= 0x80;
    }
    else if (edc_socket_endpoints.size() > 0)
    {
        main_args.edc_socket_endpoint = edc_socket_endpoints[0];
    }
    else
    {
        main_args.edc_library_path = edc_library_paths[0];
    }

    main_args.edc_json_format = args["--enable-edc-json-format"].asBool();

    std::string edc_lib_load_method_str(args["--edc-library-load-method"].asString());
    if (edc_lib_load_method_str == "dlmopen")
    {
        main_args.edc_library_load_method = EdcLibraryLoadMethod::DLMOPEN;
    }
    else if (edc_lib_load_method_str == "dlopen")
    {
        main_args.edc_library_load_method = EdcLibraryLoadMethod::DLOPEN;
    }
    else
    {
        XBT_ERROR("Invalid parameter given to --edc-library-load-method: '%s'.", edc_lib_load_method_str.c_str());
        error = true;
        return_code |= 0x80;
    }

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
    main_args.profile_reuse_enabled = args["--enable-profile-reuse"].asBool();

    if (main_args.profile_reuse_enabled && !main_args.dynamic_registration_enabled)
    {
        XBT_ERROR("Profile reuse is enabled but dynamic registration is not, have you missed something?");
        error = true;
    }

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

    // Raw argv
    // ********
    main_args.raw_argv.reserve(argc);
    for (int i = 0 ; i < argc; ++i)
    {
        main_args.raw_argv.push_back(std::string(argv[i]));
    }

    // Other options
    // *************
    main_args.dump_execution_context = args["--dump-execution-context"].asBool();
    main_args.allow_compute_sharing = args["--enable-compute-sharing"].asBool();
    main_args.allow_storage_sharing = !(args["--disable-storage-sharing"].asBool());
    if (!main_args.eventList_descriptions.empty())
    {
        main_args.forward_unknown_events = args["--forward-unknown-events"].asBool();
    }
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

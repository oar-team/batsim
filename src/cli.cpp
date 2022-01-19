#include "cli.hpp"

#include "docopt/docopt.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include <simgrid/s4u.hpp>
#include <simgrid/version.h>

#include <CLI/CLI.hpp>

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(cli, "cli"); //!< Logging

// Do not move this class to cli.hpp, it has been put here to optimize compilation time (to avoid the inclusion of CLI.hpp that is heavy on templates).
class MyFormatter : public CLI::Formatter
{
public:
    MyFormatter() : Formatter() {}

    std::string make_option_opts(const CLI::Option * opt) const override
    {
        std::stringstream out;

        if(!opt->get_option_text().empty())
        {
            out << " " << opt->get_option_text();
        } else
        {
            if(opt->get_type_size() != 0)
            {
                if(!opt->get_type_name().empty())
                    out << " " << get_label(opt->get_type_name());
                if(!opt->get_default_str().empty())
                    out << "=" << opt->get_default_str();
                if(opt->get_expected_max() == CLI::detail::expected_max_vector_size)
                    out << " ...";
                else if(opt->get_expected_min() > 1)
                    out << " x " << opt->get_expected();

                if(opt->get_required())
                    out << " " << get_label("REQUIRED");
            }
        }
        return out.str();
    }

    std::string make_group(std::string group, bool is_positional, std::vector<const CLI::Option *> opts) const override
    {
        std::stringstream out;

        out << group << ":\n";
        for(const CLI::Option *opt : opts)
        {
            out << make_option(opt, is_positional);
        }

        return out.str();
    }

    std::string make_examples(std::string name) const
    {
        std::stringstream out;
        out << "Usage examples:\n";
        out << "      " << (name.empty() ? "" : " ") << name << " -p ./platform.xml -w ./workload.json -l /path/to/fcfs.so 0 ''\n";
        out << "      " << (name.empty() ? "" : " ") << name << " -p ./platform.xml -W ./workflow.dax -S 'tcp://localhost:28000' 1 ./edc-conf-file.dhall\n";

        return out.str();
    }

    std::string make_help(const CLI::App * app, std::string name, CLI::AppFormatMode mode) const override
    {
        std::stringstream out;

        out << make_description(app) << '\n';
        out << make_usage(app, name);
        out << make_examples(name);
        out << make_positionals(app) << '\n';
        out << make_groups(app, mode);

        return out.str();
    }
};

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
    CLI::App app{"Infrastructure simulator for job and I/O scheduling"};
    auto formatter = std::make_shared<MyFormatter>();
    app.formatter(formatter);

    // Input
    const std::string input_group_name = "Input options";
    std::string platform_file;
    app.add_option("-p,--platform", platform_file, "The SimGrid platform to simulate — cf. https://batsim.rtfd.io/en/latest/input-platform.html")
        ->group(input_group_name)
        ->option_text("<file>")
        ->check(CLI::ExistingFile);

    std::vector<std::string> workload_files;
    app.add_option("-w,--workload", workload_files, "A workload JSON file to simulate — cf. https://batsim.rtfd.io/en/latest/input-workload.html")
        ->group(input_group_name)
        ->option_text("<file>...")
        ->check(CLI::ExistingFile);

    std::vector<std::string> workflow_files;
    app.add_option("-W,--workflow", workflow_files, "A workflow XML file to simulate — cf. https://pegasus.isi.edu/documentation/development/schemas.html")
        ->group(input_group_name)
        ->option_text("<file>...")
        ->check(CLI::ExistingFile);

    std::vector<std::tuple<std::string, double> > cut_workflow_files;
    app.add_option("--WS,--workflow-start", cut_workflow_files, "Same as --workflow, but the workflow starts at <start-time> instead of 0")
        ->group(input_group_name)
        ->option_text("(<file> <start-time>)...");

    std::vector<std::string> external_events_files;
    app.add_option("--events", external_events_files, "A file containing external events to inject in the simulation")
        ->group(input_group_name)
        ->option_text("<file>...")
        ->check(CLI::ExistingFile);

    // Output
    const std::string output_group_name = "Output options";
    std::string export_prefix = "out";
    app.add_option("-e,--export", export_prefix, "The export filename prefix used to generate simulation outputs. Default: out/")
        ->group(output_group_name)
        ->option_text("<prefix>");

    bool trace_machine_state = false;
    app.add_flag("--trace-machine-state", trace_machine_state, "Enable the generation of output file that traces machine states over time")
        ->group(output_group_name);

    bool trace_pstate_change = false;
    app.add_flag("--trace-pstate-change", trace_pstate_change, "Enable the generation of output file that traces machine pstate changes over time")
        ->group(output_group_name);

    ProbeTracingStrategy probe_tracing_strategy = ProbeTracingStrategy::AS_PROBE_REQUESTED;
    std::map<std::string, ProbeTracingStrategy> pts_map{{"always", ProbeTracingStrategy::ALWAYS}, {"never", ProbeTracingStrategy::NEVER}, {"auto", ProbeTracingStrategy::AS_PROBE_REQUESTED}};
    app.add_option("--trace-probe-data", probe_tracing_strategy, "")
        ->group(output_group_name)
        ->option_text("<when>")
        ->description("Force tracing of data generated by probes. Accepted values: {always, never, auto}\nDefault (auto) will trace probes that request to be traced")
        ->transform(CLI::CheckedTransformer(pts_map, CLI::ignore_case));

    // External decision components
    const std::string edc_group_name = "External decision component (EDC) options";
    std::vector<std::tuple<std::string, bool, std::string> > edc_lib_strings;
    app.add_option("-l,--edc-library-str", edc_lib_strings, "")
        ->group(edc_group_name)
        ->option_text("(<lib-path> <json-format-bool> <init-str>)...")
        ->description("Add an EDC as a library loaded by Batsim and called through a C API\n<lib-path> is the path of the library to load\n<json-format-bool> sets format of batprotocol messages (1->JSON, 0->binary)\nContent of <init-str> string is the EDC initialization buffer");

    std::vector<std::tuple<std::string, bool, std::string> > edc_lib_files;
    app.add_option("-L,--edc-library-file", edc_lib_strings, "")
        ->group(edc_group_name)
        ->option_text("(<lib-path> <json-format-bool> <init-file>)...")
        ->description("Same as --edc-library-str but content of <init-file> file is the EDC initialization buffer");

    std::vector<std::tuple<std::string, bool, std::string> > edc_socket_strings;
    app.add_option("-s,--edc-socket-str", edc_socket_strings, "")
        ->group(edc_group_name)
        ->option_text("(<socket-endpoint> <json-format-bool> <init-str>)...")
        ->description("Same as --edc-library-str but the EDC is a process called through RPC via ZeroMQ\nBatsim does not run the process, this should be done by the user\nExample <socket-endpoint> value: 'tcp://localhost:28000'");

    std::vector<std::tuple<std::string, bool, std::string> > edc_socket_files;
    app.add_option("-S,--edc-socket-file", edc_socket_files, "")
        ->group(edc_group_name)
        ->option_text("(<socket-endpoint> <json-format-bool> <init-file>)...")
        ->description("Same as --edc-library-file but the EDC is added as a process called through RPC via ZeroMQ");

    EdcLibraryLoadMethod edc_lib_load_method = EdcLibraryLoadMethod::DLMOPEN;
    std::map<std::string, EdcLibraryLoadMethod> ellm_map{{"dlmopen", EdcLibraryLoadMethod::DLMOPEN}, {"dlopen", EdcLibraryLoadMethod::DLOPEN}};
    app.add_option("--edc-library-load-method", edc_lib_load_method, "How to load EDC libraries in memory. Accepted values: {dlmopen, dlopen}. Default: dlmopen")
        ->group(edc_group_name)
        ->option_text("<method>")
        ->transform(CLI::CheckedTransformer(ellm_map, CLI::ignore_case));

    // Platform
    const std::string platform_group_name = "Platform options";
    std::string master_host = "master_host";
    app.add_option("-m,--master-host", master_host, "The SimGrid host where misc. simulation actors will be run. Default: master_host")
        ->group(platform_group_name)
        ->option_text("<hostname>");

    std::vector<std::tuple<std::string, std::string> > roles_to_add;
    app.add_option("-r,--add-role", roles_to_add, "Add a role to a host. Accepted roles: {master, storage, compute_node}")
        ->group(platform_group_name)
        ->option_text("(<hostname> <role>)...");

    uint32_t platform_mmax = 0;
    app.add_option("--mmax", platform_mmax, "Limits the number of machines to <nb>. 0 (default) means no limit")
        ->group(platform_group_name)
        ->option_text("<nb>");

    bool platform_mmax_workload = false;
    app.add_flag("--mmax-workload", platform_mmax_workload, "If set, limits the number of machines to the 'nb_res' field of the input workloads\nIf several workloads are used, the maximum value of these fields is kept")
        ->group(platform_group_name);

    // Simulation model
    const std::string simulation_model_group_name = "Simulation model options";
    bool energy_host = false;
    bool energy_link = false;
    bool energy_host_and_link = false;

    app.add_flag("--energy-host", energy_host, "Enable the SimGrid host_energy plugin")
        ->group(simulation_model_group_name);
    app.add_flag("--energy-link", energy_link, "Enable the SimGrid link_energy plugin")
        ->group(simulation_model_group_name);
    app.add_flag("-E,--energy", energy_host_and_link, "Shortcut for --energy-host --energy-link")
        ->group(simulation_model_group_name)
        ->excludes("--energy-host")
        ->excludes("--energy-link");
    std::vector<std::string> simgrid_configs;
    app.add_option("--sg-cfg", simgrid_configs, "Set a SimGrid configuration variable — cf. https://simgrid.org/doc/latest/Configuring_SimGrid.html#existing-configuration-items")
        ->group(simulation_model_group_name)
        ->option_text("<name:value>...");

    // Verbosity
    const std::string verbosity_group_name = "Verbosity and debuggability options";
    VerbosityLevel verbosity_level = VerbosityLevel::INFORMATION;
    std::map<std::string, VerbosityLevel> vl_map{{"quiet", VerbosityLevel::QUIET}, {"info", VerbosityLevel::INFORMATION}, {"debug", VerbosityLevel::DEBUG}};
    app.add_option("-v,--verbosity", verbosity_level, "Sets verbosity level. Accepted values: {quiet, info, debug}. Default: info")
        ->group(verbosity_group_name)
        ->option_text("<level>")
        ->transform(CLI::CheckedTransformer(vl_map, CLI::ignore_case));

    bool quiet = false;
    app.add_flag("-q,--quiet", quiet, "Shortcut for --verbosity quiet")
        ->group(verbosity_group_name)
        ->excludes("--verbosity");

    std::vector<std::string> simgrid_logging_controls;
    app.add_option("--sg-log", simgrid_logging_controls, "Set a SimGrid logging value — cf. https://simgrid.org/doc/latest/Configuring_SimGrid.html#logging-configuration")
        ->group(verbosity_group_name)
        ->option_text("<cat.key:value>");

    // Workflow
    const std::string workflow_group_name = "Workflow options";
    uint32_t workflow_job_limit = 0;
    app.add_option("--workflow-jobs-limit", workflow_job_limit, "Limit the number of concurrent jobs for workflows. 0 (default) means no limit")
        ->group(workflow_group_name)
        ->option_text("<nb>");

    bool ignore_beyond_last_workflow = false;
    app.add_flag("--skip-jobs-after-workflows", ignore_beyond_last_workflow, "Skip workload job submissions after all workflows have completed")
        ->group(workflow_group_name);

    // Configuration file
    const std::string config_group_name = "Configuration file options";
    app.set_config("-c,--config", "", "Read Batsim CLI options from configuration <file> as TOML/INI format")
        ->group(config_group_name)
        ->configurable(false)
        ->option_text("<file>");

    std::string output_configuration_file;
    app.add_option("--gen-config", output_configuration_file, "Generate configuration <file> from the other CLI arguments of this program call")
        ->group(config_group_name)
        ->configurable(false)
        ->option_text("<file>");

    // Misc.
    const std::string misc_group_name = "Misc. options";
    app.set_help_flag("-h,--help", "Print this help message and exit")
        ->group(misc_group_name)
        ->configurable(false);

    bool batsim_version = false;
    app.add_flag("--version", batsim_version, "Print Batsim version and exit")
        ->group(misc_group_name)
        ->configurable(false)
        ->excludes("--help");

    bool batsim_commit = false;
    app.add_flag("--git-commit", batsim_commit, "Print Batsim git commit and exit")
        ->group(misc_group_name)
        ->configurable(false)
        ->excludes("--help")
        ->excludes("--version");

    bool simgrid_version = false;
    app.add_flag("--simgrid-version", simgrid_version, "Print SimGrid version and exit")
        ->group(misc_group_name)
        ->configurable(false)
        ->excludes("--help")
        ->excludes("--version")
        ->excludes("--git-commit");

    bool dump_execution_context = false;
    app.add_flag("--dump-execution-context", dump_execution_context, "Print Batsim execution context as JSON and exit")
        ->group(misc_group_name)
        ->configurable(false)
        ->excludes("--help")
        ->excludes("--version")
        ->excludes("--git-commit")
        ->excludes("--simgrid-version");

    try
    {
        app.parse(argc, argv);
    }
    catch(const CLI::ParseError & e)
    {
        return_code = app.exit(e);
        run_simulation = false;
        return;
    }

    for (const auto & edc_lib_string : edc_lib_strings)
    {
        printf("edc-lib: (path='%s', bool=%s, init-cfg='%s')\n",
            std::get<0>(edc_lib_string).c_str(),
            std::to_string(std::get<1>(edc_lib_string)).c_str(),
            std::get<2>(edc_lib_string).c_str()
        );
    }

    return_code = 1;
    run_simulation = false;

    // TODO: change hosts format in roles to support intervals
    // The <hosts> should be formated as follow:
    // hostname[intervals],hostname[intervals],...
    // Where `intervals` is a comma separated list of simple integer
    // or closed interval separated with a '-'.
    // Example: -r host[1-5,8]:role1,role2 -r toto,tata:myrole

/*
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

    main_args.host_energy_used = args["--energy"].asBool();


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
*/
}

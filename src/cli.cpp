#include "cli.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include <simgrid/s4u.hpp>
#include <simgrid/version.h>

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>

using namespace std;

/**
 * @brief Reads a whole file and return its content as a string.
 * @param[in] filename The file to read.
 * @return The file content, as a string.
 */
static std::string read_whole_file_as_string(const std::string & filename)
{
    std::ifstream f(filename);
    if (!f.is_open())
    {
        throw std::runtime_error("cannot read scheduler configuration file '" + filename + "'");
    }

    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

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

void parse_main_args(int argc, char * argv[], MainArguments & main_args, int & return_code, bool & run_simulation, bool & only_print_information)
{
    bool error = false;
    return_code = 1;
    run_simulation = false;
    only_print_information = false;
    const char * error_prefix = "Command-line parsing error: ";

    CLI::App app{"Infrastructure simulator for job and I/O scheduling"};
    auto formatter = std::make_shared<MyFormatter>();
    app.formatter(formatter);

    // Input
    const std::string input_group_name = "Input options";
    app.add_option("-p,--platform", main_args.platform_filename, "The SimGrid platform to simulate — cf. https://batsim.rtfd.io/en/latest/input-platform.html")
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
    app.add_option("-e,--export", main_args.export_prefix, "The export filename prefix used to generate simulation outputs. Default: out/")
        ->group(output_group_name)
        ->option_text("<prefix>");

    app.add_flag("--trace-machine-state", main_args.enable_machine_state_tracing, "Enable the generation of output file that traces machine states over time")
        ->group(output_group_name);

    app.add_flag("--trace-pstate-change", main_args.enable_pstate_change_tracing, "Enable the generation of output file that traces machine pstate changes over time")
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
        ->description("Add an EDC as a library loaded by Batsim and called through a C API\n<lib-path> is the path of the library to load\n<json-format-bool> sets format of batprotocol messages (0->binary, 1->JSON)\nContent of <init-str> string is the EDC initialization buffer");

    std::vector<std::tuple<std::string, bool, std::string> > edc_lib_files;
    app.add_option("-L,--edc-library-file", edc_lib_files, "")
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

    std::map<std::string, EdcLibraryLoadMethod> ellm_map{{"dlmopen", EdcLibraryLoadMethod::DLMOPEN}, {"dlopen", EdcLibraryLoadMethod::DLOPEN}};
    app.add_option("--edc-library-load-method", main_args.edc_library_load_method, "How to load EDC libraries in memory. Accepted values: {dlmopen, dlopen}. Default: dlopen")
        ->group(edc_group_name)
        ->option_text("<method>")
        ->transform(CLI::CheckedTransformer(ellm_map, CLI::ignore_case));

    // Platform
    const std::string platform_group_name = "Platform options";
    app.add_option("-m,--master-host", main_args.master_host_name, "The SimGrid host where misc. simulation actors will be run. Default: master_host")
        ->group(platform_group_name)
        ->option_text("<hostname>");

    std::vector<std::tuple<std::string, std::string> > roles_to_add;
    app.add_option("-r,--add-role", roles_to_add, "Add a role to a host. Accepted roles: {master, storage, compute_node}")
        ->group(platform_group_name)
        ->option_text("(<hostname> <role>)...");

    app.add_option("--mmax", main_args.limit_machines_count, "Limits the number of machines to <nb>. 0 (default) means no limit")
        ->group(platform_group_name)
        ->option_text("<nb>");

    app.add_flag("--mmax-workload", main_args.limit_machines_count_by_workload, "If set, limits the number of machines to the 'nb_res' field of the input workloads\nIf several workloads are used, the maximum value of these fields is kept")
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

    app.add_option("--sg-cfg", main_args.simgrid_config, "Set a SimGrid configuration variable — cf. https://simgrid.org/doc/latest/Configuring_SimGrid.html#existing-configuration-items")
        ->group(simulation_model_group_name)
        ->option_text("<name:value>...");

    // Verbosity
    const std::string verbosity_group_name = "Verbosity and debuggability options";
    std::map<std::string, VerbosityLevel> vl_map{{"quiet", VerbosityLevel::QUIET}, {"info", VerbosityLevel::INFORMATION}, {"debug", VerbosityLevel::DEBUG}};
    app.add_option("-v,--verbosity", main_args.verbosity, "Sets verbosity level. Accepted values: {quiet, info, debug}. Default: info")
        ->group(verbosity_group_name)
        ->option_text("<level>")
        ->transform(CLI::CheckedTransformer(vl_map, CLI::ignore_case));

    bool quiet = false;
    app.add_flag("-q,--quiet", quiet, "Shortcut for --verbosity quiet")
        ->group(verbosity_group_name)
        ->excludes("--verbosity");

    app.add_option("--sg-log", main_args.simgrid_logging, "Set a SimGrid logging value — cf. https://simgrid.org/doc/latest/Configuring_SimGrid.html#logging-configuration")
        ->group(verbosity_group_name)
        ->option_text("<cat.key:value>");

    // Workflow
    const std::string workflow_group_name = "Workflow options";
    app.add_option("--workflow-jobs-limit", main_args.workflow_nb_concurrent_jobs_limit, "Limit the number of concurrent jobs for workflows. 0 (default) means no limit")
        ->group(workflow_group_name)
        ->option_text("<nb>");

    app.add_flag("--skip-jobs-after-workflows", main_args.terminate_with_last_workflow, "Skip workload job submissions after all workflows have completed")
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

    app.add_flag("--batsim-version,--version", main_args.print_batsim_version, "Print Batsim version and exit")
        ->group(misc_group_name)
        ->configurable(false);

    app.add_flag("--batsim-git-commit", main_args.print_batsim_commit, "Print Batsim git commit and exit")
        ->group(misc_group_name)
        ->configurable(false);

    app.add_flag("--simgrid-version", main_args.print_simgrid_version, "Print SimGrid version and exit")
        ->group(misc_group_name)
        ->configurable(false);

    app.add_flag("--simgrid-git-commit", main_args.print_simgrid_commit, "Print SimGrid git commit and exit")
        ->group(misc_group_name)
        ->configurable(false);

    app.add_flag("--dump-execution-context", main_args.dump_execution_context, "Print Batsim execution context as JSON and exit")
        ->group(misc_group_name)
        ->configurable(false);

    try
    {
        app.parse(argc, argv);
    }
    catch(const CLI::ParseError & e)
    {
        return_code = app.exit(e);
        run_simulation = false;
        only_print_information = false;

        return;
    }

    // Should the simulation be run?
    int nb_stopping_flags = static_cast<int>(main_args.dump_execution_context) +
        static_cast<int>(main_args.print_batsim_version) +
        static_cast<int>(main_args.print_batsim_commit) +
        static_cast<int>(main_args.print_simgrid_version) +
        static_cast<int>(main_args.print_simgrid_commit);
    if (nb_stopping_flags > 1)
    {
        fprintf(stderr, "%sOnly one of the flags that print information and exit should be set.\n", error_prefix);
        error = true;
    }
    only_print_information = (nb_stopping_flags == 1);

    // write configuration to file if --gen-config is used
    if (!output_configuration_file.empty())
    {
        if (file_exists(output_configuration_file))
        {
            fprintf(stderr, "WARNING in command line parsing: writing configuration to already existing file '%s'.", output_configuration_file.c_str());
        }

        // Check if INI format is required
        if (std::filesystem::path(output_configuration_file).extension() == ".ini")
        {
            app.config_formatter(std::make_shared<CLI::ConfigINI>());
        }

        //std::string output_configuration_str = app.config_to_str();
        std::ofstream ofstream(output_configuration_file);
        ofstream << app.config_to_str();
        ofstream.close();
    }

    // Workloads
    for (size_t i = 0; i < workload_files.size(); i++)
    {
        const string & workload_file = workload_files[i];

        MainArguments::WorkloadDescription desc;
        desc.filename = absolute_filename(workload_file);
        desc.name = string("w") + to_string(i);

        main_args.workload_descriptions.push_back(desc);
    }

    // Workflows (with default start time)
    for (size_t i = 0; i < workflow_files.size(); i++)
    {
        const string & workflow_file = workflow_files[i];

        MainArguments::WorkflowDescription desc;
        desc.filename = absolute_filename(workflow_file);
        desc.name = string("wf") + to_string(i);
        desc.workload_name = desc.name;
        desc.start_time = 0;

        main_args.workflow_descriptions.push_back(desc);
    }

    // Workflows (with user-given start time)
    for (unsigned int i = 0; i < cut_workflow_files.size(); ++i)
    {
        const string & cut_workflow_file = std::get<0>(cut_workflow_files[i]);
        const double & cut_workflow_start_time = std::get<1>(cut_workflow_files[i]);

        MainArguments::WorkflowDescription desc;
        desc.filename = absolute_filename(cut_workflow_file);
        desc.name = string("wfc") + to_string(i);
        desc.workload_name = desc.name;
        desc.start_time = cut_workflow_start_time;

        bool local_error = false;
        if (!file_exists(cut_workflow_file))
        {
            fprintf(stderr, "%s--workflow-start <file> '%s' cannot be read.\n", error_prefix, cut_workflow_file.c_str());
            local_error = true;
        }
        if (desc.start_time < 0)
        {
            fprintf(stderr, "%s--workflow-start <start_time> should be positive, but %g was given.\n", error_prefix, desc.start_time);
            local_error = true;
        }

        if (local_error)
        {
            error = true;
        }
        else
        {
            main_args.workflow_descriptions.push_back(desc);
        }
    }

    // External events
    for (size_t i = 0; i < external_events_files.size(); i++)
    {
        const string & events_file = external_events_files[i];

        MainArguments::EventListDescription desc;
        desc.filename = absolute_filename(events_file);
        desc.name = string("e") + to_string(i);

        main_args.eventList_descriptions.push_back(desc);
    }

    // Platform
    if (!only_print_information && main_args.platform_filename == "")
    {
        fprintf(stderr, "%sThe SimGrid platform has not been set.\n", error_prefix);
        error = true;
    }

    main_args.hosts_roles_map[main_args.master_host_name] = "master";
    for (size_t i = 0; i < roles_to_add.size(); i++)
    {
        const string & hostname = std::get<0>(roles_to_add[i]);
        const string & role = std::get<1>(roles_to_add[i]);
        main_args.hosts_roles_map[hostname] = role;
    }

    main_args.host_energy_used = energy_host;
    if (energy_link)
    {
        fprintf(stderr, "%s--energy-link is not implemented.\n", error_prefix);
        error = true;
    }
    if (energy_host_and_link)
    {
        fprintf(stderr, "%s--energy is not implemented.\n", error_prefix);
        error = true;
    }

    // EDCs
    const auto nb_edc = edc_lib_files.size() + edc_lib_strings.size() + edc_socket_files.size() + edc_socket_strings.size();
    if (!only_print_information) {
        if (nb_edc == 0)
        {
            fprintf(stderr, "%sAt least one external decision component (EDC) should be set.\n", error_prefix);
            error = true;
        }
        else if (nb_edc > 1)
        {
            fprintf(stderr, "%sUsing several external decision components (EDCs) in a single simulation is not implemented.\n", error_prefix);
            error = true;
        }
        else if(edc_socket_strings.size() > 0)
        {
            main_args.edc_socket_endpoint = std::get<0>(edc_socket_strings[0]);
            main_args.edc_json_format = std::get<1>(edc_socket_strings[0]);
            main_args.edc_init_buffer = std::get<2>(edc_socket_strings[0]);
        }
        else if(edc_socket_files.size() > 0)
        {
            main_args.edc_socket_endpoint = std::get<0>(edc_socket_files[0]);
            main_args.edc_json_format = std::get<1>(edc_socket_files[0]);
            main_args.edc_init_buffer = read_whole_file_as_string(std::get<2>(edc_socket_files[0]));
        }
        else if (edc_lib_strings.size() > 0)
        {
            main_args.edc_library_path = std::get<0>(edc_lib_strings[0]);
            main_args.edc_json_format = std::get<1>(edc_lib_strings[0]);
            main_args.edc_init_buffer = std::get<2>(edc_lib_strings[0]);
        }
        else if (edc_lib_files.size() > 0)
        {
            main_args.edc_library_path = std::get<0>(edc_lib_files[0]);
            main_args.edc_json_format = std::get<1>(edc_lib_files[0]);
            main_args.edc_init_buffer = read_whole_file_as_string(std::get<2>(edc_lib_files[0]));
        }
    }

    // Verbosity
    if (quiet)
        main_args.verbosity = VerbosityLevel::QUIET;

    // Raw argv
    main_args.raw_argv.reserve(argc);
    for (int i = 0 ; i < argc; ++i)
    {
        main_args.raw_argv.push_back(std::string(argv[i]));
    }

    run_simulation = !error && (nb_stopping_flags == 0);

    if (!error)
    {
        return_code = 0;
    }
    else
    {
        fprintf(stderr, "Aborting as there were errors. Run with --help for more information.\n");
    }
    fflush(stderr);
}

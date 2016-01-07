#include <string>

#include <stdio.h>
#include <argp.h>
#include <unistd.h>

#include <simgrid/msg.h>
#include <simgrid/plugins.h>

#include <boost/algorithm/string/case_conv.hpp>

#include "context.hpp"
#include "export.hpp"
#include "ipp.hpp"
#include "job_submitter.hpp"
#include "jobs.hpp"
#include "jobs_execution.hpp"
#include "machines.hpp"
#include "network.hpp"
#include "profiles.hpp"
#include "server.hpp"
#include "workload.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "batsim");

enum class VerbosityLevel
{
    QUIET,
    NETWORK_ONLY,
    INFORMATION,
    DEBUG
};

/**
 * @brief The main function arguments (a.k.a. program arguments)
 */
struct MainArguments
{
    std::string platformFilename;   //! The SimGrid platform filename
    std::string workloadFilename;   //! The JSON workload filename

    std::string socketFilename;     //! The Unix Domain Socket filename

    std::string masterHostName;     //! The name of the SimGrid host which runs scheduler processes and not user tasks
    std::string exportPrefix;       //! The filename prefix used to export simulation information

    bool energy_used;               //! True if and only if the SimGrid energy plugin should be used.
    VerbosityLevel verbosity;       //! Sets the Batsim verbosity

    bool abort;                     //! A boolean value. If set to yet, the launching should be aborted for reason abortReason
    std::string abortReason;        //! Human readable reasons which explains why the launch should be aborted
};

/**
 * @brief Used to parse the main function parameters
 * @param[in] key The current key
 * @param[in] arg The current argument
 * @param[in, out] state The current argp_state
 * @return 0
 */
int parse_opt (int key, char *arg, struct argp_state *state)
{
    MainArguments * mainArgs = (MainArguments *) state->input;

    switch (key)
    {
    case 'e':
        mainArgs->exportPrefix = arg;
        break;
    case 'm':
        mainArgs->masterHostName = arg;
        break;
    case 'p':
        mainArgs->energy_used = true;
        break;
    case 'v':
    {
        string sArg = arg;
        boost::to_lower(sArg);
        if (sArg == "quiet")
            mainArgs->verbosity = VerbosityLevel::QUIET;
        else if (sArg == "network-only")
            mainArgs->verbosity = VerbosityLevel::NETWORK_ONLY;
        else if (sArg == "information")
            mainArgs->verbosity = VerbosityLevel::INFORMATION;
        else if (sArg == "debug")
            mainArgs->verbosity = VerbosityLevel::DEBUG;
        else
        {
            mainArgs->abort = true;
            mainArgs->abortReason += "\n  invalid VERBOSITY_LEVEL argument: '" + string(sArg) + "' is not in [quiet, network-only, information, debug].";
        }
        break;
    }
    case 'q':
        mainArgs->verbosity = VerbosityLevel::QUIET;
        break;
    case 's':
        mainArgs->socketFilename = arg;
        break;
    case ARGP_KEY_ARG:
        switch(state->arg_num)
        {
        case 0:
            mainArgs->platformFilename = arg;
            if (access(mainArgs->platformFilename.c_str(), R_OK) == -1)
            {
                mainArgs->abort = true;
                mainArgs->abortReason += "\n  invalid PLATFORM_FILE argument: file '" + string(mainArgs->platformFilename) + "' cannot be read";
            }
            break;
        case 1:
            mainArgs->workloadFilename = arg;
            if (access(mainArgs->workloadFilename.c_str(), R_OK) == -1)
            {
                mainArgs->abort = true;
                mainArgs->abortReason += "\n invalid WORKLOAD_FILE argument: file '" + string(mainArgs->workloadFilename) + "' cannot be read";
            }
            break;
        }
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 2)
        {
            mainArgs->abort = 1;
            mainArgs->abortReason += "\n\tToo few arguments. Try the --help option to display usage information.";
        }
        /*else if (state->arg_num > 2)
        {
            mainArgs->abort = 1;
            strcat(mainArgs->abortReason, "\n\tToo many arguments.");
        }*/
        break;
    }

    return 0;
}

int main(int argc, char * argv[])
{
    // TODO : boolean option to enable the energy plugin
    MainArguments mainArgs;
    mainArgs.socketFilename = "/tmp/bat_socket";
    mainArgs.masterHostName = "master_host";
    mainArgs.exportPrefix = "out";
    mainArgs.energy_used = false;
    mainArgs.verbosity = VerbosityLevel::INFORMATION;
    mainArgs.abort = false;

    struct argp_option options[] =
    {
        {"socket", 's', "FILENAME", 0, "Unix Domain Socket filename", 0},
        {"master-host", 'm', "NAME", 0, "The name of the host in PLATFORM_FILE which will run SimGrid scheduling processes and won't be used to compute tasks", 0},
        {"export", 'e', "FILENAME_PREFIX", 0, "The export filename prefix used to generate simulation output", 0},
        {"energy-plugin", 'p', 0, 0, "Enables energy-aware experiments", 0},
        {"verbosity", 'v', "VERBOSITY_LEVEL", 0, "Sets the Batsim verbosity level. Available values are : quiet, network-only, information (default), debug.", 0},
        {"quiet", 'q', 0, 0, "Shortcut for --verbosity=quiet", 0},
        {0, '\0', 0, 0, 0, 0} // The options array must be NULL-terminated
    };
    struct argp argp = {options, parse_opt, "PLATFORM_FILE WORKLOAD_FILE", "A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.", 0, 0, 0};
    argp_parse(&argp, argc, argv, 0, 0, &mainArgs);

    if (mainArgs.abort)
    {
        fprintf(stderr, "Impossible to run batsim:%s\n", mainArgs.abortReason.c_str());
        return 1;
    }

    if (mainArgs.energy_used)
        sg_energy_plugin_init();

    if (mainArgs.verbosity == VerbosityLevel::QUIET || mainArgs.verbosity == VerbosityLevel::NETWORK_ONLY)
    {
        xbt_log_control_set("workload.thresh:error");
        xbt_log_control_set("jobs.thresh:error");
        xbt_log_control_set("batsim.thresh:error");
        xbt_log_control_set("machines.thresh:error");
        xbt_log_control_set("pstate.thresh:error");
        xbt_log_control_set("jobs_execution.thresh:error");
        xbt_log_control_set("export.thresh:error");
        xbt_log_control_set("profiles.thresh:error");
        xbt_log_control_set("network.thresh:error");
        xbt_log_control_set("server.thresh:error");
        xbt_log_control_set("ipp.thresh:error");
    }

    if (mainArgs.verbosity == VerbosityLevel::NETWORK_ONLY)
    {
        xbt_log_control_set("network.thresh:info");
    }
    else if (mainArgs.verbosity == VerbosityLevel::DEBUG)
    {
        xbt_log_control_set("workload.thresh:debug");
        xbt_log_control_set("jobs.thresh:debug");
        xbt_log_control_set("batsim.thresh:debug");
        xbt_log_control_set("machines.thresh:debug");
        xbt_log_control_set("pstate.thresh:debug");
        xbt_log_control_set("jobs_execution.thresh:debug");
        xbt_log_control_set("export.thresh:debug");
        xbt_log_control_set("profiles.thresh:debug");
        xbt_log_control_set("network.thresh:debug");
        xbt_log_control_set("server.thresh:debug");
        xbt_log_control_set("ipp.thresh:debug");
    }

    // Initialization
    MSG_init(&argc, argv);

    BatsimContext context;
    context.platform_filename = mainArgs.platformFilename;
    context.workload_filename = mainArgs.workloadFilename;
    context.energy_used = mainArgs.energy_used;

    load_json_workload(&context, mainArgs.workloadFilename);
    context.jobs.setProfiles(&context.profiles);
    context.tracer.setFilename(mainArgs.exportPrefix + "_schedule.trace");
    //context.jobs.displayDebug();

    XBT_INFO("Checking whether SMPI is used or not...");
    bool smpi_used = context.jobs.containsSMPIJob();
    if (!smpi_used)
    {
        XBT_INFO("SMPI will NOT be used.");
        MSG_config("host/model", "ptask_L07");
    }
    else
        XBT_INFO("SMPI will be used.");

    XBT_INFO("Creating the machines...");
    MSG_create_environment(mainArgs.platformFilename.c_str());

    xbt_dynar_t hosts = MSG_hosts_as_dynar();
    context.machines.createMachines(hosts, &context, mainArgs.masterHostName);
    xbt_dynar_free(&hosts);
    const Machine * masterMachine = context.machines.masterMachine();
    context.machines.setTracer(&context.tracer);
    context.tracer.initialize(&context, MSG_get_clock());
    XBT_INFO("Machines created successfully. There are %lu computing machines.", context.machines.machines().size());

    // Socket
    context.socket.create_socket(mainArgs.socketFilename);
    context.socket.accept_pending_connection();

    // Main processes running
    XBT_INFO("Creating jobs_submitter process...");
    JobSubmitterProcessArguments * submitterArgs = new JobSubmitterProcessArguments;
    submitterArgs->context = &context;
    MSG_process_create("jobs_submitter", job_submitter_process, (void*)submitterArgs, masterMachine->host);
    XBT_INFO("The jobs_submitter process has been created.");

    XBT_INFO("Creating the uds_server process...");
    ServerProcessArguments * serverArgs = new ServerProcessArguments;
    serverArgs->context = &context;
    MSG_process_create("server", uds_server_process, (void*)serverArgs, masterMachine->host);
    XBT_INFO("The uds_server process has been created.");

    msg_error_t res = MSG_main();

    // Finalization
    context.tracer.finalize(&context, MSG_get_clock());
    exportScheduleToCSV(mainArgs.exportPrefix + "_schedule.csv", MSG_get_clock(), &context);
    exportJobsToCSV(mainArgs.exportPrefix + "_jobs.csv", &context);

    if (res == MSG_OK)
        return 0;
    else
        return 1;

    return 0;
}

/**
 * @file batsim.cpp
 * @brief Batsim's entry point
 */

#include <string>

#include <stdio.h>
#include <argp.h>
#include <unistd.h>

#include <simgrid/msg.h>
#include <smpi/smpi.h>
#include <simgrid/plugins/energy.h>

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

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "batsim"); //!< Logging

/**
 * @brief Batsim verbosity level
 */
enum class VerbosityLevel
{
    QUIET           //!< Almost nothing should be displayed
    ,NETWORK_ONLY   //!< Only network messages should be displayed
    ,INFORMATION    //!< Informations should be displayed (default)
    ,DEBUG          //!< Debug informations should be displayed too
};

/**
 * @brief The main function arguments (a.k.a. program arguments)
 */
struct MainArguments
{
    std::string platformFilename;                           //!< The SimGrid platform filename
    std::string workloadFilename;                           //!< The JSON workload filename
    std::string workflowFilename;                           //!< The workflow description filename

    std::string socketFilename = "/tmp/bat_socket";         //!< The Unix Domain Socket filename

    std::string masterHostName = "master_host";             //!< The name of the SimGrid host which runs scheduler processes and not user tasks
    std::string exportPrefix = "out";                       //!< The filename prefix used to export simulation information

    std::string redis_hostname = "127.0.0.1";               //!< The Redis (data storage) server host name
    int redis_port = 6379;                                  //!< The Redis (data storage) server port

    int limit_machines_count = -1;                          //!< The number of machines to use to compute jobs. -1 : no limit. >= 1 : the number of computation machines
    bool limit_machines_count_by_workload = false;          //!< If set to true, the number of computing machiens to use should be limited by the workload description

    bool energy_used = false;                               //!< True if and only if the SimGrid energy plugin should be used.
    VerbosityLevel verbosity = VerbosityLevel::INFORMATION; //!< Sets the Batsim verbosity
    bool allow_space_sharing = false;                       //!< Allows/forbids space sharing. Two jobs can run on the same machine if and only if space sharing is allowed.
    bool enable_simgrid_process_tracing = false;            //!< If set to true, this options enables the tracing of SimGrid processes
    bool enable_schedule_tracing = true;                    //!< If set to true, the schedule is exported to a Pajé trace file

    bool abort = false;                                     //!< A boolean value. If set to yet, the launching should be aborted for reason abortReason
    std::string abortReason;                                //!< Human readable reasons which explains why the launch should be aborted
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
    case 'h':
        mainArgs->allow_space_sharing = true;
        break;
    case 'H':
        mainArgs->redis_hostname = arg;
        break;
    case 'e':
        mainArgs->exportPrefix = arg;
        break;
    case 'l':
    {
        int ivalue = stoi(arg);

        if ((ivalue < -1) || (ivalue == 0))
        {
            mainArgs->abort = true;
            mainArgs->abortReason += "\n  invalid M positional argument (" + to_string(ivalue) + "): it should either be -1 or a strictly positive value";
        }

        mainArgs->limit_machines_count = ivalue;
        break;
    }
    case 'L':
    {
        mainArgs->limit_machines_count_by_workload = true;
        break;
    }
    case 'm':
        mainArgs->masterHostName = arg;
        break;
    case 'E':
        mainArgs->energy_used = true;
        break;
    case 'P':
    {
        int ivalue = stoi(arg);

        if ((ivalue <= 0) || (ivalue > 65535))
        {
            mainArgs->abort = true;
            mainArgs->abortReason += "\n  invalid PORT positional argument (" + to_string(ivalue) +
                                     "): it should be a valid port: integer in range [1,65535].";
        }

        mainArgs->redis_port = ivalue;
        break;
    }
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
            mainArgs->abortReason += "\n  invalid VERBOSITY_LEVEL argument: '" + string(sArg) + "' is not in {quiet, network-only, information, debug}.";
        }
        break;
    }
    case 'q':
        mainArgs->verbosity = VerbosityLevel::QUIET;
        break;
    case 's':
        mainArgs->socketFilename = arg;
        break;
    case 'p':
        mainArgs->platformFilename = arg;
      if (access(mainArgs->platformFilename.c_str(), R_OK) == -1)
	{
	  mainArgs->abort = true;
	  mainArgs->abortReason += "\n  invalid PLATFORM_FILE argument: file '" + string(mainArgs->platformFilename) + "' cannot be read";
	}
  break;
    case 'w':
      mainArgs->workloadFilename = arg;
      if (access(mainArgs->workloadFilename.c_str(), R_OK) == -1)
	{
	  mainArgs->abort = true;
	  mainArgs->abortReason += "\n  invalid WORKLOAD_FILE argument: file '" + string(mainArgs->workloadFilename) + "' cannot be read";
	}
break;
    case 'W':
      mainArgs->workflowFilename = arg;
      if (access(mainArgs->workflowFilename.c_str(), R_OK) == -1)
	{
	  mainArgs->abort = true;
	  mainArgs->abortReason += "\n  invalid WORKFLOW_FILE argument: file '" + string(mainArgs->workflowFilename) + "' cannot be read";
	}
      break;
    case 't':
        mainArgs->enable_simgrid_process_tracing = true;
        break;
    case 'T':
        mainArgs->enable_schedule_tracing = false;
        break;
    // case ARGP_KEY_ARG:
    //     switch(state->arg_num)
    //     {
    //     case 0:
    //         mainArgs->platformFilename = arg;
    //         if (access(mainArgs->platformFilename.c_str(), R_OK) == -1)
    //         {
    //             mainArgs->abort = true;
    //             mainArgs->abortReason += "\n  invalid PLATFORM_FILE argument: file '" + string(mainArgs->platformFilename) + "' cannot be read";
    //         }
    //         break;
    //     case 1:
    //         mainArgs->workloadFilename = arg;
    //         if (access(mainArgs->workloadFilename.c_str(), R_OK) == -1)
    //         {
    //             mainArgs->abort = true;
    //             mainArgs->abortReason += "\n  invalid WORKLOAD_FILE argument: file '" + string(mainArgs->workloadFilename) + "' cannot be read";
    //         }
    //         break;
    //     }
    //     break;
    // case ARGP_KEY_END:
    //     if (state->arg_num < 2)
    //     {
    //         mainArgs->abort = true;
    //         mainArgs->abortReason += "\n  Too few arguments. Try the --help option to display usage information.";
    //     }
    //     break;
	// To be removed after code review
    }

    return 0;
}

/**
 * @brief Main function
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0 on success, something else otherwise
 */
int main(int argc, char * argv[])
{
    MainArguments mainArgs;

    struct argp_option options[] =
    {
        {"platform", 'p', "FILENAME", 0, "Platform to be simulated by SimGrid. Required.", 0},
        {"workload", 'w', "FILENAME", 0, "Workload to be submitted to the platform. Required unless a workflow is submitted.", 0},
        {"workflow", 'W', "FILENAME", 0, "Workflow to be submitted to the platform. Required unless a workload is submitted.", 0},
        {"export", 'e', "FILENAME_PREFIX", 0, "The export filename prefix used to generate simulation output. Default value: 'out'", 0},
        {"allow-space-sharing", 'h', 0, 0, "Allows space sharing: the same resource can compute several jobs at the same time", 0},
        {"redis-hostname", 'H', "HOSTNAME", 0, "Sets the host name of the remote Redis (data storage) server.", 0},
        {"limit-machine-count", 'l', "M", 0, "Allows to limit the number of computing machines to use. If M == -1 (default), all the machines described in PLATFORM_FILE are used (but the master_host). If M >= 1, only the first M machines will be used to comupte jobs.", 0},
        {"limit-machine-count-by-worload", 'L', 0, 0, "If set, allows to limit the number of computing machines to use. This number is read from the workload file. If both limit-machine-count and limit-machine-count-by-worload are set, the minimum of the two will be used.", 0},
        {"master-host", 'm', "NAME", 0, "The name of the host in PLATFORM_FILE which will run SimGrid scheduling processes and won't be used to compute tasks. Default value: 'master_host'", 0},
        {"redis-port", 'P', "PORT", 0, "Sets the port of the remote Redis (data storage) server.", 0},
        {"energy-plugin", 'E', 0, 0, "Enables energy-aware experiments", 0},
        {"quiet", 'q', 0, 0, "Shortcut for --verbosity=quiet", 0},
        {"socket", 's', "FILENAME", 0, "Unix Domain Socket filename. Default value: '/tmp/bat_socket'", 0},
        {"process-tracing", 't', 0, 0, "Enables SimGrid process tracing (shortcut for SimGrid options ----cfg=tracing:1 --cfg=tracing/msg/process:1)", 0},
        {"disable-schedule-tracing", 'T', 0, 0, "If set, the tracing of the schedule is disabled (the output file _schedule.trace will not be exported)", 0},
        {"verbosity", 'v', "VERBOSITY_LEVEL", 0, "Sets the Batsim verbosity level. Available values are : quiet, network-only, information (default), debug.", 0},
        {0, '\0', 0, 0, 0, 0} // The options array must be NULL-terminated
    };
    struct argp argp = {options, parse_opt, 0, "A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.", 0, 0, 0};
    argp_parse(&argp, argc, argv, 0, 0, &mainArgs);

    if (access(mainArgs.platformFilename.c_str(), R_OK) == -1)
      {
	mainArgs.abort = true;
	mainArgs.abortReason += "\n  invalid PLATFORM_FILE argument: file '" + string(mainArgs.platformFilename) + "' cannot be read";
      }

    
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

    // Setting SimGrid configuration if the SimGrid process tracing is enabled
    if (mainArgs.enable_simgrid_process_tracing)
    {
        string sg_trace_filename = mainArgs.exportPrefix + "_sg_processes.trace";

        MSG_config("tracing", "1");
        MSG_config("tracing/msg/process", "1");
        MSG_config("tracing/filename", sg_trace_filename.c_str());
    }

    BatsimContext context;
    context.platform_filename = mainArgs.platformFilename;
    context.workload_filename = mainArgs.workloadFilename;
    context.export_prefix = mainArgs.exportPrefix;
    context.energy_used = mainArgs.energy_used;
    context.allow_space_sharing = mainArgs.allow_space_sharing;
    context.trace_schedule = mainArgs.enable_schedule_tracing;

    // Loading the static workload
    int nb_machines_by_workload;
    const string static_workload_name = "static";
    Workload * static_workload = new Workload;
    static_workload->load_from_json(mainArgs.workloadFilename, nb_machines_by_workload);
    context.workloads.insert_workload(static_workload_name, static_workload);

    int limit_machines_count = -1;
    if ((mainArgs.limit_machines_count_by_workload) && (mainArgs.limit_machines_count > 0))
        limit_machines_count = min(mainArgs.limit_machines_count, nb_machines_by_workload);
    else if (mainArgs.limit_machines_count_by_workload)
        limit_machines_count = nb_machines_by_workload;
    else if (mainArgs.limit_machines_count > 0)
        limit_machines_count = mainArgs.limit_machines_count;

    if (limit_machines_count != -1)
        XBT_INFO("The number of machines will be limited to %d", limit_machines_count);

    XBT_INFO("Checking whether SMPI is used or not...");
    context.smpi_used = static_workload->jobs->containsSMPIJob();
    if (!context.smpi_used)
    {
        XBT_INFO("SMPI will NOT be used.");
        MSG_config("host/model", "ptask_L07");
    }
    else
    {
        XBT_INFO("SMPI will be used.");
        static_workload->register_smpi_applications();
        SMPI_init();
    }

    if (context.trace_schedule)
        context.paje_tracer.setFilename(mainArgs.exportPrefix + "_schedule.trace");

    XBT_INFO("Creating the machines...");
    MSG_create_environment(mainArgs.platformFilename.c_str());

    xbt_dynar_t hosts = MSG_hosts_as_dynar();
    context.machines.createMachines(hosts, &context, mainArgs.masterHostName, limit_machines_count);
    xbt_dynar_free(&hosts);
    const Machine * masterMachine = context.machines.masterMachine();
    if (context.trace_schedule)
    {
        context.machines.setTracer(&context.paje_tracer);
        context.paje_tracer.initialize(&context, MSG_get_clock());
    }
    XBT_INFO("Machines created successfully. There are %lu computing machines.", context.machines.machines().size());

    if (context.energy_used)
    {
        // Energy consumption tracing
        context.energy_tracer.set_context(&context);
        context.energy_tracer.set_filename(mainArgs.exportPrefix + "_consumed_energy.csv");

        // Power state tracing
        context.pstate_tracer.setFilename(mainArgs.exportPrefix + "_pstate_changes.csv");

        std::map<int, MachineRange> pstate_to_machine_set;
        for (const Machine * machine : context.machines.machines())
        {
            int machine_id = machine->id;
            int pstate = MSG_host_get_pstate(machine->host);

            if (pstate_to_machine_set.count(pstate) == 0)
            {
                MachineRange range;
                range.insert(machine_id);
                pstate_to_machine_set[pstate] = range;
            }
            else
                pstate_to_machine_set[pstate].insert(machine_id);
        }

        for (auto mit : pstate_to_machine_set)
        {
            int pstate = mit.first;
            MachineRange & range = mit.second;
            context.pstate_tracer.add_pstate_change(MSG_get_clock(), range, pstate);
        }
    }

    // Socket
    context.socket.create_socket(mainArgs.socketFilename);
    context.socket.accept_pending_connection();

    // Redis
    context.storage.set_instance_key_prefix(absolute_filename(mainArgs.socketFilename));
    context.storage.connect_to_server(mainArgs.redis_hostname, mainArgs.redis_port);

    // Starting the simulated processes 

    XBT_INFO("Creating jobs_submitter process...");
    JobSubmitterProcessArguments * submitterArgs = new JobSubmitterProcessArguments;
    submitterArgs->context = &context;
    submitterArgs->workload_name = static_workload_name;
    MSG_process_create("jobs_submitter", static_job_submitter_process, (void*)submitterArgs, masterMachine->host);
    XBT_INFO("The jobs_submitter process has been created.");

    if (! mainArgs.workflowFilename.empty()) {
      XBT_INFO("Creating workflow_submitter process...");
      WorkflowSubmitterProcessArguments * submitterArgs = new WorkflowSubmitterProcessArguments;
      submitterArgs->context = &context;
      submitterArgs->workflow_filename = mainArgs.workflowFilename;
      MSG_process_create("workflow_submitter", workflow_submitter_process, (void*)submitterArgs, masterMachine->host);
      XBT_INFO("The workflow_submitter process has been created.");
    }


    XBT_INFO("Creating the uds_server process...");
    ServerProcessArguments * serverArgs = new ServerProcessArguments;
    serverArgs->context = &context;
    MSG_process_create("server", uds_server_process, (void*)serverArgs, masterMachine->host);
    XBT_INFO("The uds_server process has been created.");

    msg_error_t res = MSG_main();

    if (context.smpi_used)
        SMPI_finalize();

    // Finalization
    if (context.trace_schedule)
        context.paje_tracer.finalize(&context, MSG_get_clock());
    exportScheduleToCSV(mainArgs.exportPrefix + "_schedule.csv", &context);
    exportJobsToCSV(mainArgs.exportPrefix + "_jobs.csv", &context);

    if (res == MSG_OK)
        return 0;
    else
        return 1;
}

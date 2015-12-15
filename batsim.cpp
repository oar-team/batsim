#include <string>

#include <stdio.h>
#include <argp.h>
#include <unistd.h>

#include <simgrid/msg.h>

#include "context.hpp"
#include "export.hpp"
#include "ipp.hpp"
#include "job_submitter.hpp"
#include "jobs.hpp"
#include "jobs_execution.hpp"
#include "machines.hpp"
#include "network.hpp"
#include "profiles.hpp"

using namespace std;

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
static int parse_opt (int key, char *arg, struct argp_state *state)
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
    MainArguments mainArgs;
    mainArgs.socketFilename = "/tmp/bat_socket";
    mainArgs.masterHostName = "master_host";
    mainArgs.exportPrefix = "out";
    mainArgs.abort = false;

    struct argp_option options[] =
    {
        {"socket", 's', "FILENAME", 0, "Unix Domain Socket filename", 0},
        {"master-host", 'm', "NAME", 0, "The name of the host in PLATFORM_FILE which will run SimGrid scheduling processes and won't be used to compute tasks", 0},
        {"export", 'e', "FILENAME_PREFIX", 0, "The export filename prefix used to generate simulation output", 0},
        {0, '\0', 0, 0, 0, 0} // The options array must be NULL-terminated
    };
    struct argp argp = {options, parse_opt, "PLATFORM_FILE WORKLOAD_FILE", "A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.", 0, 0, 0};
    argp_parse(&argp, argc, argv, 0, 0, &mainArgs);

    if (mainArgs.abort)
    {
        fprintf(stderr, "Impossible to run batsim:%s\n", mainArgs.abortReason.c_str());
        return 1;
    }

    // Initialization
    MSG_init(&argc, argv);

    BatsimContext context;
    context.jobs.load_from_json(mainArgs.workloadFilename);
    context.profiles.load_from_json(mainArgs.workloadFilename);
    context.jobs.setProfiles(&context.profiles);
    context.tracer.setFilename(mainArgs.exportPrefix + "_schedule.trace");
    // TODO: check jobs & profile validity

    bool smpi_used = context.jobs.containsSMPIJob();
    if (!smpi_used)
        MSG_config("host/model", "ptask_L07");

    MSG_create_environment(mainArgs.platformFilename.c_str());

    xbt_dynar_t hosts = MSG_hosts_as_dynar();
    context.machines.createMachines(hosts, mainArgs.masterHostName);
    xbt_dynar_free(&hosts);
    const Machine * masterMachine = context.machines.masterMachine();

    // Socket
    context.socket.create_socket(mainArgs.socketFilename);
    context.socket.accept_pending_connection();

    // Main processes running
    JobSubmitterProcessArguments * submitterArgs = new JobSubmitterProcessArguments;
    submitterArgs->context = &context;
    MSG_process_create("jobs_submitter", job_submitter_process, (void*)submitterArgs, masterMachine->host);

    ServerProcessArguments * serverArgs = new ServerProcessArguments;
    serverArgs->context = &context;
    MSG_process_create("server", uds_server_process, (void*)serverArgs, masterMachine->host);

    msg_error_t res = MSG_main();

    // Finalization
    exportScheduleToCSV(mainArgs.exportPrefix + "_schedule.csv", MSG_get_clock(), &context);

    if (res == MSG_OK)
        return 0;
    else
        return 1;

    return 0;
}

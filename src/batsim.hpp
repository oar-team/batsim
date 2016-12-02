#pragma once

#include <string>
#include <list>

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
 * @brief Generates a uuid string
 * @param[in] string_to_hash T
 * @param[in] output_length The maximum length of the output string
 * @pre output_length > 0 && output_length < 20
 * @post result.size() == output_length
 * @return The generated hash (truncated to output_length characters if needed)
 */
std::string generate_sha1_string(std::string string_to_hash, int output_length = 6);

/**
 * @brief Stores Batsim's arguments, a.k.a. the main function arguments
 */
struct MainArguments
{
    /**
     * @brief Stores the command-line description of a workload
     */
    struct WorkloadDescription
    {
        std::string filename;   //!< The name of the workload file
        std::string name;       //!< The name of the workload
    };

    /**
     * @brief Stores the command-line description of a workflow
     */
    struct WorkflowDescription
    {
        std::string filename;       //!< The name of the workflow file
        std::string name;           //!< The name of the workflow
        std::string workload_name;  //!< The name of the workload associated with the workflow
        double start_time;          //!< The moment in time at which the workflow should be started
    };

    std::string platform_filename;                          //!< The SimGrid platform filename
    std::list<WorkloadDescription> workload_descriptions;   //!< The workloads' descriptions
    std::list<WorkflowDescription> workflow_descriptions;   //!< The workflows' descriptions
    bool terminate_with_last_workflow = false;              //!< If true, allows to ignore the jobs submitted after the last workflow termination

    std::string socket_filename = "/tmp/bat_socket";        //!< The Unix Domain Socket filename

    std::string master_host_name = "master_host";           //!< The name of the SimGrid host which runs scheduler processes and not user tasks
    std::string export_prefix;                              //!< The filename prefix used to export simulation information

    std::string pfs_host_name = "pfs_host";                 //!< The name of the SimGrid host which serves as parallel file system

    std::string redis_hostname = "127.0.0.1";               //!< The Redis (data storage) server host name
    int redis_port = 6379;                                  //!< The Redis (data storage) server port
    std::string redis_prefix;                               //!< The Redis (data storage) instance prefix

    int limit_machines_count = -1;                          //!< The number of machines to use to compute jobs. -1 : no limit. >= 1 : the number of computation machines
    bool limit_machines_count_by_workload = false;          //!< If set to true, the number of computing machiens to use should be limited by the workload description

    bool energy_used = false;                               //!< True if and only if the SimGrid energy plugin should be used.
    VerbosityLevel verbosity = VerbosityLevel::INFORMATION; //!< Sets the Batsim verbosity
    bool allow_space_sharing = false;                       //!< Allows/forbids space sharing. Two jobs can run on the same machine if and only if space sharing is allowed.
    bool enable_simgrid_process_tracing = false;            //!< If set to true, this option enables the tracing of SimGrid processes
    bool enable_schedule_tracing = true;                    //!< If set to true, the schedule is exported to a Pajé trace file
    bool enable_machine_state_tracing = true;               //!< If set to true, this option enables the tracing of the machine states into a CSV time series.

    bool abort = false;                                     //!< A boolean value. If set to yet, the launching should be aborted for reason abortReason
    std::string abortReason;                                //!< Human readable reasons which explains why the launch should be aborted
    int workflow_limit = 0;                                 //!< limit on workflow needs
};


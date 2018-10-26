#pragma once

#include <string>
#include <list>
#include <map>

#include <rapidjson/document.h>

struct BatsimContext;

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

enum class ProgramType
{
    BATSIM      //!< Classical Batsim executable
    ,BATEXEC    //!< Batexec: Simpler execution, without scheduler, socket nor redis
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
 * @brief Stores Batsim arguments, a.k.a. the main function arguments
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

    // Input
    std::string platform_filename;                          //!< The SimGrid platform filename
    std::list<WorkloadDescription> workload_descriptions;   //!< The workloads descriptions
    std::list<WorkflowDescription> workflow_descriptions;   //!< The workflows descriptions

    // Common
    std::string master_host_name;                           //!< The name of the SimGrid host which runs scheduler processes and not user tasks
    bool energy_used;                                       //!< True if and only if the SimGrid energy plugin should be used.
    std::map<std::string, std::string> hosts_roles_map;     //!< The hosts/roles mapping to be added to the hosts properties.

    // Execution context
    std::string socket_endpoint;                            //!< The Decision process socket endpoint
    bool redis_enabled;                                     //!< Whether Redis is enabled
    std::string redis_hostname;                             //!< The Redis (data storage) server host name
    int redis_port;                                         //!< The Redis (data storage) server port
    std::string redis_prefix;                               //!< The Redis (data storage) instance prefix

    // Job related
    bool forward_profiles_on_submission;                    //!< Stores whether the profile information of submitted jobs should be sent to the scheduler
    bool dynamic_submission_enabled;                        //!< Stores whether the scheduler will be able to send jobs along the simulation
    bool ack_dynamic_submission;                            //!< Stores whether Batsim will acknowledge dynamic job submission (emit JOB_SUBMITTED events)

    // Output
    std::string export_prefix;                              //!< The filename prefix used to export simulation information
    bool enable_simgrid_process_tracing;                    //!< If set to true, this option enables the tracing of SimGrid processes
    bool enable_schedule_tracing;                           //!< If set to true, the schedule is exported to a Pajé trace file
    bool enable_machine_state_tracing;                      //!< If set to true, this option enables the tracing of the machine states into a CSV time series.

    // Platform size limit
    int limit_machines_count;                               //!< The number of machines to use to compute jobs. 0 : no limit. > 0 : the number of computation machines
    bool limit_machines_count_by_workload;                  //!< If set to true, the number of computing machiens to use should be limited by the workload description

    // Verbosity
    VerbosityLevel verbosity;                               //!< Sets the Batsim verbosity

    // Workflow
    int workflow_nb_concurrent_jobs_limit = 0;              //!< Limits the number of concurrent jobs for workflows
    bool terminate_with_last_workflow;                      //!< If true, allows to ignore the jobs submitted after the last workflow termination

    // Other
    bool allow_time_sharing;                                //!< Allows/forbids time sharing. Two jobs can run on the same machine if and only if time sharing is allowed.
    ProgramType program_type;                               //!< The program type (Batsim or Batexec at the moment)
    std::string pfs_host_name;                              //!< The name of the SimGrid host which serves as parallel file system (a.k.a. large-capacity storage tier)
    std::string hpst_host_name;                             //!< The name of the SimGrid host which serves as the high-performance storage tier
};

/**
 * @brief Parses Batsim command-line arguments
 * @param[in] argc The number of arguments given to the main function
 * @param[in] argv The values of arguments given to the main function
 * @param[out] main_args Batsim usable arguments
 * @param[out] return_code Batsim's return code (used directly if false is returned)
 * @param[out] run_simulation Whether the simulation should be run afterwards
 * @param[out] run_unit_tests Whether the unit tests should be run afterwards
 */
void parse_main_args(int argc, char * argv[], MainArguments & main_args,
                     int & return_code, bool & run_simulation, bool & run_unit_tests);

/**
 * @brief Configures how the simulation should be logged
 * @param[in,out] main_args Batsim arguments
 */
void configure_batsim_logging_output(const MainArguments & main_args);

/**
 * @brief Loads the workloads defined in Batsim arguments
 * @param[in] main_args Batsim arguments
 * @param[in,out] context The BatsimContext
 * @param[out] max_nb_machines_to_use The maximum number of machines that should be used in the simulation.
 *             This number is computed from Batsim arguments but depends on Workloads content. -1 means no limitation.
 */
void load_workloads_and_workflows(const MainArguments & main_args, BatsimContext * context, int & max_nb_machines_to_use);

/**
 * @brief Starts the SimGrid processes that should be executed at the beginning of the simulation
 * @param[in] main_args Batsim arguments
 * @param[in] context The BatsimContext
 * @param[in] is_batexec If set to true, only workloads are handled, the server is not launched
 *            and simpler workload submitters are used
 */
void start_initial_simulation_processes(const MainArguments & main_args,
                                        BatsimContext * context,
                                        bool is_batexec = false);

/**
 * @brief Sets the simulation configuration
 * @param[in,out] context The BatsimContext
 * @param[in] main_args Batsim arguments
 */
void set_configuration(BatsimContext * context,
                       MainArguments & main_args);

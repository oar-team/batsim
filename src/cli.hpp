/**
 * @file cli.hpp
 * @brief Contains types and functions related to Batsim's Command-Line Interface
 */

#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

/** @def STR_HELPER(x)
 *  @brief Helper macro to retrieve the string view of a macro.
 */
#define STR_HELPER(x) #x

/** @def STR(x)
 *  @brief Macro to get a const char* from a macro
 */
#define STR(x) STR_HELPER(x)

/**
 * @brief Batsim verbosity level
 */
enum class VerbosityLevel
{
    QUIET           //!< Almost nothing should be displayed
    ,INFORMATION    //!< Informations should be displayed (default)
    ,DEBUG          //!< Debug informations should be displayed too
};

/**
 * @brief Batsim program type
 */
enum class ProgramType
{
    BATSIM      //!< Classical Batsim executable
    ,BATEXEC    //!< Batexec: Simpler execution, without external scheduler
};

/**
 * @brief The internal method used to load external decision components
 */
enum class EdcLibraryLoadMethod
{
    DLMOPEN //!< Use dlmopen to load libraries in distinct namespaces.
    ,DLOPEN //!< Use dlopen to load libraries in the default namespace.
};

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

   /**
    * @brief Stores the command-line description of an eventList
    */
   struct EventListDescription
   {
       std::string filename;        //!< The filename of the eventList
       std::string name;            //!< The name of the eventList
   };

    // Input
    std::string platform_filename;                          //!< The SimGrid platform filename
    std::list<WorkloadDescription> workload_descriptions;   //!< The workloads descriptions
    std::list<WorkflowDescription> workflow_descriptions;   //!< The workflows descriptions
    std::list<EventListDescription> eventList_descriptions; //!< The descriptions of the eventLists

    // Common
    std::string master_host_name;                           //!< The name of the SimGrid host which runs scheduler processes and not user tasks
    bool host_energy_used = false;                          //!< True if and only if the SimGrid host_energy plugin should be used.
    std::map<std::string, std::string> hosts_roles_map;     //!< The hosts/roles mapping to be added to the hosts properties.

    // Execution context
    std::string edc_socket_endpoint;                        //!< The External Decision Component process socket endpoint. Empty if unset.
    std::string edc_library_path;                           //!< The External Decision Component library path. Empty if unset.
    bool edc_json_format = false;                           //!< If true, messages to communicate with EDCs should be sent as JSON strings.

    // Output
    std::string export_prefix;                              //!< The filename prefix used to export simulation information
    bool enable_schedule_tracing = false;                   //!< If set to true, the schedule is exported to a Pajé trace file
    bool enable_machine_state_tracing = false;              //!< If set to true, this option enables the tracing of the machine states into a CSV time series.

    // Platform size limit
    int limit_machines_count = 0;                           //!< The number of machines to use to compute jobs. 0 : no limit. > 0 : the number of computation machines
    bool limit_machines_count_by_workload = false;          //!< If set to true, the number of computing machiens to use should be limited by the workload description

    // Verbosity
    VerbosityLevel verbosity = VerbosityLevel::QUIET;       //!< Sets the Batsim verbosity

    // Workflow
    int workflow_nb_concurrent_jobs_limit = 0;              //!< Limits the number of concurrent jobs for workflows
    bool terminate_with_last_workflow = false;              //!< If true, allows to ignore the jobs submitted after the last workflow termination

    // Raw argv
    std::vector<std::string> raw_argv;                      //!< The strings the Batsim process received as argv.

    // Other
    std::vector<std::string> simgrid_config;                //!< The list of configuration options to pass to SimGrid.
    std::vector<std::string> simgrid_logging;               //!< The list of simulation logging options to pass to SimGrid.
    bool dump_execution_context = false;                    //!< Instead of running the simulation, print the execution context as JSON on the standard output.
    bool allow_compute_sharing = false;                     //!< Allows/forbids sharing on compute machines. Two jobs can run concurrently on the same machine if and only if sharing is allowed.
    bool allow_storage_sharing = false;                     //!< Allows/forbids sharing on storage machines. Two jobs can run concurrently on the same machine if and only if sharing is allowed.
    bool forward_unknown_events = false;                    //!< Whether the unknown external events should be forwarded to the scheduler.
    EdcLibraryLoadMethod edc_library_load_method;           //!< How external decision components should be loaded in memory.
    ProgramType program_type = ProgramType::BATSIM;         //!< The program type (Batsim or Batexec at the moment)
    std::string pfs_host_name;                              //!< The name of the SimGrid host which serves as parallel file system (a.k.a. large-capacity storage tier)
    std::string hpst_host_name;                             //!< The name of the SimGrid host which serves as the high-performance storage tier

public:
    /**
     * @brief Generate a JSON string that describes Batsim's execution context
     * @return A JSON string that describes Batsim's execution context
     */
    std::string generate_execution_context_json() const;
};


/**
 * @brief Parses Batsim command-line arguments
 * @param[in] argc The number of arguments given to the main function
 * @param[in] argv The values of arguments given to the main function
 * @param[out] main_args Batsim usable arguments
 * @param[out] return_code Batsim's return code (used directly if false is returned)
 * @param[out] run_simulation Whether the simulation should be run afterwards
 */
void parse_main_args(int argc, char * argv[], MainArguments & main_args,
                     int & return_code, bool & run_simulation);

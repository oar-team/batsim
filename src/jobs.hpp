/**
 * @file jobs.hpp
 * @brief Contains job-related structures
 */

#pragma once

#include <map>
#include <vector>
#include <deque>

#include <rapidjson/document.h>

#include <simgrid/msg.h>

#include "exact_numbers.hpp"
#include "machine_range.hpp"

using namespace std;

class Profiles;
struct Profile;
class Workload;
struct Job;

/**
 * @brief A simple structure used to identify one job
 */
struct JobIdentifier
{
    /**
     * @brief Creates a JobIdentifier
     * @param[in] workload_name The workload name
     * @param[in] job_number The job number
     */
    JobIdentifier(const std::string & workload_name = "", int job_number = -1);

    std::string workload_name; //!< The name of the workload the job belongs to
    int job_number; //!< The job unique number inside its workload

    /**
     * @brief Returns a string representation of the JobIdentifier.
     * @details Output format is WORKLOAD_NAME!JOB_NUMBER
     * @return A string representation of the JobIdentifier.
     */
    std::string to_string() const;
};

/**
 * @brief Compares two JobIdentifier thanks to their string representations
 * @param[in] ji1 The first JobIdentifier
 * @param[in] ji2 The second JobIdentifier
 * @return ji1.to_string() < ji2.to_string()
 */
bool operator<(const JobIdentifier & ji1, const JobIdentifier & ji2);


/**
 * @brief Contains the different states a job can be in
 */
enum class JobState
{
     JOB_STATE_NOT_SUBMITTED          //!< The job exists but cannot be scheduled yet.
    ,JOB_STATE_SUBMITTED              //!< The job has been submitted, it can now be scheduled.
    ,JOB_STATE_RUNNING                //!< The job has been scheduled and is currently being processed.
    ,JOB_STATE_COMPLETED_SUCCESSFULLY //!< The job execution finished before its walltime successfully.
    ,JOB_STATE_COMPLETED_FAILED       //!< The job execution finished before its walltime but the job failed.
    ,JOB_STATE_COMPLETED_WALLTIME_REACHED       //!< The job has reached his walltime and have been killed
    ,JOB_STATE_COMPLETED_KILLED       //!< The job has been killed.
    ,JOB_STATE_REJECTED               //!< The job has been rejected by the scheduler.
};


/**
 *  @brief Internal batsim simulation task: the job profile instantiation.
 */
struct BatTask
{
    BatTask(Job * parent_job, Profile * profile);
    ~BatTask();

    Job * parent_job;
    Profile * profile; //!< The job profile. The corresponding profile tells how the job should be computed
    // Manage MSG profiles
    msg_task_t ptask = nullptr; //!< The final task to execute (only set for the leaf of the BatTask tree)
    // manage Delay profile
    double delay_task_start = -1; //!< Keep delay task starting time in order to compute progress afterwards
    double delay_task_required = -1; //!< Keep delay task time requirement

    // manage sequential profile
    vector<BatTask*> sub_tasks; //!< List of sub task of this task to be executed sequentially or in parallel depending on the profile
    unsigned int current_task_index = -1; //!< index in the sub_tasks vector of the current task
    double current_task_progress_ratio = 0; //!< give the progress of the current task from 0 to 1
    void compute_tasks_progress();

    private:
      void compute_leaf_progress();
};


/**
 * @brief Represents a job
 */
struct Job
{
    ~Job();

    Workload * workload = nullptr; //!< The workload the job belongs to
    int number; //!< The job unique number within its workload
    JobIdentifier id; //!< The job unique identifier
    std::string profile; //!< The job profile name. The corresponding profile tells how the job should be computed
    Rational submission_time; //!< The job submission time: The time at which the becomes available
    Rational walltime; //!< The job walltime: if the job is executed for more than this amount of time, it will be killed
    int required_nb_res; //!< The number of resources the job is requested to be executed on
    std::string kill_reason; //!< If the job has been killed, the kill reason is stored in this variable

    bool kill_requested = false; //!< Whether the job kill has been requested

    std::set<msg_process_t> execution_processes; //!< The processes involved in running the job

    std::string json_description; //!< The JSON description of the job

    long double consumed_energy; //!< The sum, for each machine on which the job has been allocated, of the consumed energy (in Joules) during the job execution time (consumed_energy_after_job_completion - consumed_energy_before_job_start)

    std::deque<std::string> incoming_message_buffer; //!< The buffer for incoming messages from the scheduler.

    Rational starting_time; //!< The time at which the job starts to be executed.
    Rational runtime; //!< The amount of time during which the job has been executed
    MachineRange allocation; //!< The machines on which the job has been executed.
    std::vector<int> smpi_ranks_to_hosts_mapping; //!< If the job uses a SMPI profile, stores which host number each MPI rank should use. These numbers must be in [0,required_nb_res[.
    JobState state; //!< The current state of the job
    int return_code = -1; //!< The return code of the job

    BatTask * task = nullptr; //!< The task to be executed by this job

    BatTask * compute_job_progress(); //<! return the task progression of the task tree

    /**
     * @brief Creates a new-allocated Job from a JSON description
     * @param[in] json_desc The JSON description of the job
     * @param[in] workload The Workload the job is in
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The newly allocated Job
     * @pre The JSON description of the job is valid
     */
    static Job * from_json(const rapidjson::Value & json_desc,
                           Workload * workload,
                           const std::string & error_prefix = "Invalid JSON job");

    /**
     * @brief Creates a new-allocated Job from a JSON description
     * @param[in] json_str The JSON description of the job (as a string)
     * @param[in] workload The Workload the job is in
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The newly allocated Job
     * @pre The JSON description of the job is valid
     */
    static Job * from_json(const std::string & json_str,
                           Workload * workload,
                           const std::string & error_prefix = "Invalid JSON job");
    /**
     * @brief Return true if the job has complete (successfully or not),
     * false if the job has not started
     */
    bool is_complete();
};


/**
 * @brief Compares job thanks to their submission times
 * @param[in] a The first job
 * @param[in] b The second job
 * @return True if and only if the first job's submission time is lower than the second job's submission time
 */
bool job_comparator_subtime_number(const Job * a, const Job * b);

/**
 * @brief Stores all the jobs of a workload
 */
class Jobs
{
public:
    /**
     * @brief Constructs an empty Jobs
     */
    Jobs();

    /**
     * @brief Jobs cannot be copied.
     * @param[in] other Another instance
     */
    Jobs(const Jobs & other) = delete;

    /**
     * @brief Destroys a Jobs
     * @details All Job instances will be deleted
     */
    ~Jobs();

    /**
     * @brief Sets the profiles which are associated to the Jobs
     * @param[in] profiles The profiles
     */
    void setProfiles(Profiles * profiles);

    /**
     * @brief Sets the Workload within which this Jobs instance exist
     * @param[in] workload The Workload
     */
    void setWorkload(Workload * workload);

    /**
     * @brief Loads the jobs from a JSON document
     * @param[in] doc The JSON document
     * @param[in] filename The name of the file the JSON document has been extracted from
     */
    void load_from_json(const rapidjson::Document & doc, const std::string & filename);

    /**
     * @brief Accesses one job thanks to its unique number
     * @param[in] job_number The job unique number
     * @return A pointer to the job associated to the given job number
     */
    Job * operator[](int job_number);

    /**
     * @brief Accesses one job thanks to its unique number (const version)
     * @param[in] job_number The job unique number
     * @return A (const) pointer to the job associated to the given job number
     */
    const Job * operator[](int job_number) const;

    /**
     * @brief Accesses one job thanks to its unique number
     * @param[in] job_number The job unique number
     * @return A pointer to the job associated to the given job number
     */
    Job * at(int job_number);

    /**
     * @brief Accesses one job thanks to its unique number (const version)
     * @param[in] job_number The job unique number
     * @return A (const) pointer to the job associated to the given job number
     */
    const Job * at(int job_number) const;

    /**
     * @brief Adds a job into a Jobs instance
     * @param[in] job The job to add
     * @pre No job with the same number exist in the Jobs instance
     */
    void add_job(Job * job);

    /**
     * @brief Allows to know whether a job exists
     * @param[in] job_number The unique job number
     * @return True if and only if a job with the given job number exists
     */
    bool exists(int job_number) const;

    /**
     * @brief Allows to know whether the Jobs contains any SMPI job
     * @return True if the number of SMPI jobs in the Jobs is greater than 0.
     */
    bool contains_smpi_job() const;

    /**
     * @brief Displays the contents of the Jobs class (debug purpose)
     */
    void displayDebug() const;

    /**
     * @brief Returns a copy of the std::map which contains the jobs
     * @return A copy of the std::map which contains the jobs
     */
    const std::map<int, Job*> & jobs() const;

    /**
     * @brief Returns a reference to the std::map which contains the jobs
     * @return A reference to the std::map which contains the jobs
     */
    std::map<int, Job*> & jobs();

    /**
     * @brief Returns the number of jobs of the Jobs instance
     * @return the number of jobs of the Jobs instance
     */
    int nb_jobs() const;

private:
    std::map<int, Job*> _jobs; //!< The std::map which contains the jobs
    Profiles * _profiles = nullptr; //!< The profiles associated with the jobs
    Workload * _workload = nullptr; //!< The Workload the jobs belong to
};

/**
 * @brief Returns a std::string corresponding to a given JobState
 * @param[in] state The JobState
 * @return A std::string corresponding to a given JobState
 */
std::string job_state_to_string(const JobState & state);

/**
 * @brief Returns a JobState corresponding to a given std::string
 * @param[in] state The std::string
 * @return A JobState corresponding to a given std::string
 */
JobState job_state_from_string(const std::string & state);

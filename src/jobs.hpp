/**
 * @file jobs.hpp
 * @brief Contains job-related structures
 */

#pragma once

#include <map>
#include <vector>

#include <rapidjson/document.h>

#include "machine_range.hpp"

class Profiles;
class Workload;

/**
 * @brief Contains the different states a job can be in
 */
enum class JobState
{
     JOB_STATE_NOT_SUBMITTED          //!< The job exists but cannot be scheduled yet.
    ,JOB_STATE_SUBMITTED              //!< The job has been submitted, it can now be scheduled.
    ,JOB_STATE_RUNNING                //!< The job has been scheduled and is currently being processed.
    ,JOB_STATE_COMPLETED_SUCCESSFULLY //!< The job execution finished before its walltime.
    ,JOB_STATE_COMPLETED_KILLED       //!< The job execution time was longer than its walltime so the job had been killed.
    ,JOB_STATE_REJECTED               //!< The job has been rejected by the scheduler.
};

/**
 * @brief Represents a job
 */
struct Job
{
    Workload * workload = nullptr; //!< The workload the job belongs to
    int number; //!< The job unique number within its workload
    std::string profile; //!< The job profile name. The corresponding profile tells how the job should be computed
    double submission_time; //!< The job submission time: The time at which the becomes available
    double walltime; //!< The job walltime: if the job is executed for more than this amount of time, it will be killed
    int required_nb_res; //!< The number of resources the job is requested to be executed on

    std::string json_description; //!< The JSON description of the job

    long double consumed_energy; //!< The sum, for each machine on which the job has been allocated, of the consumed energy (in Joules) during the job execution time (consumed_energy_after_job_completion - consumed_energy_before_job_start)

    double starting_time; //!< The time at which the job starts to be executed.
    double runtime; //!< The amount of time during which the job has been executed
    MachineRange allocation; //!< The machines on which the job has been executed.
    JobState state; //!< The current state of the job

};

/**
 * @brief Compares job thanks to their submission times
 * @param[in] a The first job
 * @param[in] b The second job
 * @return True if and only if the first job's submission time is lower than the second job's submission time
 */
bool job_comparator_subtime(const Job * a, const Job * b);

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
     * @brief Destroys a Jobs
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
     * @param[in] job_id The job unique number
     * @return A pointer to the job associated to the given job number
     */
    Job * operator[](int job_id);

    /**
     * @brief Accesses one job thanks to its unique number (const version)
     * @param[in] job_id The job unique number
     * @return A (const) pointer to the job associated to the given job number
     */
    const Job * operator[](int job_id) const;

    /**
     * @brief Accesses one job thanks to its unique number
     * @param[in] job_id The job unique number
     * @return A pointer to the job associated to the given job number
     */
    Job * at(int job_id);

    /**
     * @brief Accesses one job thanks to its unique number (const version)
     * @param[in] job_id The job unique number
     * @return A (const) pointer to the job associated to the given job number
     */
    const Job * at(int job_id) const;

    /**
     * @brief Allows to know whether a job exists
     * @param[in] job_id The unique job number
     * @return True if and only if a job with the given job number exists
     */
    bool exists(int job_id) const;

    /**
     * @brief Allows to know whether the Jobs contains any SMPI job
     * @return True if the number of SMPI jobs in the Jobs is greater than 0.
     */
    bool containsSMPIJob() const;

    /**
     * @brief Displays the contents of the Jobs class (debug purpose)
     */
    void displayDebug() const;

    /**
     * @brief Returns a copy of the std::map which contains the jobs
     * @return A copy of the std::map which contains the jobs
     */
    const std::map<int, Job*> & jobs() const;

private:
    std::map<int, Job*> _jobs; //!< The std::map which contains the jobs
    Profiles * _profiles = nullptr; //!< The profiles associated with the jobs
    Workload * _workload = nullptr; //!< The Workload the jobs belong to
};

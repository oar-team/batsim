/**
 * @file jobs.hpp
 */

#pragma once

#include <map>
#include <vector>

#include <rapidjson/document.h>

#include "machine_range.hpp"

class Profiles;

enum class JobState
{
     JOB_STATE_NOT_SUBMITTED          //!< The job exists but cannot be scheduled yet.
    ,JOB_STATE_SUBMITTED              //!< The job has been submitted, it can now be scheduled.
    ,JOB_STATE_RUNNING                //!< The job has been scheduled and is currently being processed.
    ,JOB_STATE_COMPLETED_SUCCESSFULLY //!< The job execution finished before its walltime.
    ,JOB_STATE_COMPLETED_KILLED       //!< The job execution time was longer than its walltime so the job had been killed.
    ,JOB_STATE_REJECTED               //!< The job has been rejected by the scheduler.
};

struct Job
{
    int id;
    std::string profile;
    double submission_time;
    double walltime;
    int required_nb_res;

    long double consumed_energy; //! The sum, for each machine on which the job has been allocated, of the consumed energy (in Joules) during the job execution time (consumed_energy_after_job_completion - consumed_energy_before_job_start)

    double starting_time;
    double runtime;
    MachineRange allocation;
    JobState state;
};

bool job_comparator_subtime(const Job * a, const Job * b);

class Jobs
{
public:
    Jobs();
    ~Jobs();

    void setProfiles(Profiles * profiles);

    void load_from_json(const rapidjson::Document & doc, const std::string & filename);

    Job * operator[](int job_id);
    const Job * operator[](int job_id) const;
    bool exists(int job_id) const;
    bool containsSMPIJob() const;

    void displayDebug() const;

    const std::map<int, Job*> & jobs() const;

private:
    std::map<int, Job*> _jobs;
    Profiles * _profiles;
};

/**
 * @file jobs.hpp
 */

#pragma once

#include <map>

enum class JobState
{
     JOB_STATE_NOT_SUBMITTED          //!< The job exists but cannot be scheduled yet.
    ,JOB_STATE_SUBMITTED              //!< The job has been submitted, it can now be scheduled.
    ,JOB_STATE_RUNNING                //!< The job has been scheduled and is currently being processed.
    ,JOB_STATE_COMPLETED_SUCCESSFULLY //!< The job execution finished before its walltime.
    ,JOB_STATE_COMPLETED_KILLED       //!< The job execution time was longer than its walltime so the job had been killed.
};

struct Job
{
    int id;
    std::string profile;
    double submission_time;
    double walltime;
    int required_nb_res;

    double starting_time;
    double runtime;
    std::vector<int> allocation;
    JobState state;
};

class Jobs
{
public:
    Jobs();
    ~Jobs();

    void load_from_json(const std::string & filename);

    Job * operator[](int job_id);
    const Job * operator[](int job_id) const;
    bool exists(int job_id) const;

    const std::map<int, Job> & jobs() const;

private:
    std::map<int, Job> _jobs;
};

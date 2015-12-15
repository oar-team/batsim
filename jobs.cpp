/**
 * @file jobs.cpp
 */

#include "jobs.hpp"

#include <string>
#include <fstream>
#include <streambuf>
#include <algorithm>

#include <simgrid/msg.h>

#include <rapidjson/document.h>

#include "profiles.hpp"

using namespace std;
using namespace rapidjson;

XBT_LOG_NEW_DEFAULT_CATEGORY(jobs, "jobs");

Jobs::Jobs()
{

}

Jobs::~Jobs()
{
    for (auto mit : _jobs)
    {
        delete mit.second;
    }
}

void Jobs::setProfiles(Profiles *profiles)
{
    _profiles = profiles;
}

void Jobs::load_from_json(const std::string &filename)
{
    // Let the file content be placed in a string
    ifstream ifile(filename);
    string content;

    ifile.seekg(0, ios::end);
    content.reserve(ifile.tellg());
    ifile.seekg(0, ios::beg);

    content.assign((std::istreambuf_iterator<char>(ifile)),
                std::istreambuf_iterator<char>());

    // JSON document creation
    Document doc;
    doc.Parse(content.c_str());

    xbt_assert(doc.IsObject());
    xbt_assert(doc.HasMember("jobs"), "Invalid JSON file '%s': the 'jobs' array is missing", filename.c_str());
    const Value & jobs = doc["jobs"];
    xbt_assert(jobs.IsArray(), "Invalid JSON file '%s': the 'jobs' member is not an array", filename.c_str());

    Job j;
    j.starting_time = -1;
    j.runtime = -1;
    j.state = JobState::JOB_STATE_NOT_SUBMITTED;

    for (SizeType i = 0; i < jobs.Size(); i++) // Uses SizeType instead of size_t
    {
        const Value & job = jobs[i];
        xbt_assert(job.IsObject(), "Invalid JSON file '%s': one job is not an object", filename.c_str());

        xbt_assert(job.HasMember("id"), "Invalid JSON file '%s': one job has no 'id' field", filename.c_str());
        xbt_assert(job["id"].IsInt(), "Invalid JSON file '%s': one job has a non-integral 'id' field ('%s')", filename.c_str(), job["id"].GetString());
        j.id = job["id"].GetInt();

        xbt_assert(job.HasMember("subtime"), "Invalid JSON file '%s': job %d has no 'subtime' field", filename.c_str(), j.id);
        xbt_assert(job["subtime"].IsNumber(), "Invalid JSON file '%s': job %d has a non-number 'subtime' field", filename.c_str(), j.id);
        j.submission_time = job["subtime"].GetDouble();

        xbt_assert(job.HasMember("walltime"), "Invalid JSON file '%s': job %d has no 'walltime' field", filename.c_str(), j.id);
        xbt_assert(job["walltime"].IsNumber(), "Invalid JSON file '%s': job %d has a non-number 'walltime' field", filename.c_str(), j.id);
        j.walltime = job["walltime"].GetDouble();

        xbt_assert(job.HasMember("res"), "Invalid JSON file '%s': job %d has no 'res' field", filename.c_str(), j.id);
        xbt_assert(job["res"].IsInt(), "Invalid JSON file '%s': job %d has a non-number 'res' field", filename.c_str(), j.id);
        j.required_nb_res = job["res"].GetInt();

        xbt_assert(job.HasMember("profile"), "Invalid JSON file '%s': job %d has no 'profile' field", filename.c_str(), j.id);
        xbt_assert(job["profile"].IsString(), "Invalid JSON file '%s': job %d has a non-string 'profile' field", filename.c_str(), j.id);
        j.profile = job["profile"].GetString();

        xbt_assert(!exists(j.id), "Invalid JSON file '%s': duplication of job id %d", filename.c_str(), j.id);
        Job * nj = new Job;
        *nj = j;
        _jobs[j.id] = nj;
    }
}

Job *Jobs::operator[](int job_id)
{
    auto it = _jobs.find(job_id);
    xbt_assert(it != _jobs.end(), "Cannot get job %d: it does not exist", job_id);
    return it->second;
}

const Job *Jobs::operator[](int job_id) const
{
    auto it = _jobs.find(job_id);
    xbt_assert(it != _jobs.end(), "Cannot get job %d: it does not exist", job_id);
    return it->second;
}

bool Jobs::exists(int job_id) const
{
    auto it = _jobs.find(job_id);
    return it != _jobs.end();
}

bool Jobs::containsSMPIJob() const
{
    for (auto & mit : _jobs)
    {
        Job * job = mit.second;
        if ((*_profiles)[job->profile]->type == ProfileType::SMPI)
            return true;
    }
    return false;
}

const std::map<int, Job* > &Jobs::jobs() const
{
    return _jobs;
}

bool job_comparator_subtime(const Job *a, const Job *b)
{
    return a->submission_time < b->submission_time;
}

/**
 * @file jobs.cpp
 * @brief Contains job-related structures
 */

#include "jobs.hpp"
#include "workload.hpp"

#include <string>
#include <fstream>
#include <streambuf>
#include <algorithm>
#include <boost/regex.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include <simgrid/msg.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "profiles.hpp"

using namespace std;
using namespace rapidjson;

XBT_LOG_NEW_DEFAULT_CATEGORY(jobs, "jobs"); //!< Logging

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

void Jobs::setWorkload(Workload *workload)
{
    _workload = workload;
}

void Jobs::load_from_json(const Document &doc, const string &filename)
{
    (void) filename; // Avoids a warning if assertions are ignored
    xbt_assert(doc.IsObject());
    xbt_assert(doc.HasMember("jobs"), "Invalid JSON file '%s': the 'jobs' array is missing", filename.c_str());
    const Value & jobs = doc["jobs"];
    xbt_assert(jobs.IsArray(), "Invalid JSON file '%s': the 'jobs' member is not an array", filename.c_str());

    for (SizeType i = 0; i < jobs.Size(); i++) // Uses SizeType instead of size_t
    {
        const Value & job_json_description = jobs[i];

        Job * j = Job::from_json(job_json_description, _workload);

        xbt_assert(!exists(j->number), "Invalid JSON file '%s': duplication of job id %d", filename.c_str(), j->number);
        _jobs[j->number] = j;
    }
}

Job *Jobs::operator[](int job_number)
{
    auto it = _jobs.find(job_number);
    xbt_assert(it != _jobs.end(), "Cannot get job %d: it does not exist", job_number);
    return it->second;
}

const Job *Jobs::operator[](int job_number) const
{
    auto it = _jobs.find(job_number);
    xbt_assert(it != _jobs.end(), "Cannot get job %d: it does not exist", job_number);
    return it->second;
}

Job *Jobs::at(int job_number)
{
    return operator[](job_number);
}

const Job *Jobs::at(int job_number) const
{
    return operator[](job_number);
}

void Jobs::add_job(Job *job)
{
    xbt_assert(!exists(job->number),
               "Bad Jobs::add_job call: A job with number=%d already exists.",
               job->number);

    _jobs[job->number] = job;
}

bool Jobs::exists(int job_number) const
{
    auto it = _jobs.find(job_number);
    return it != _jobs.end();
}

bool Jobs::containsSMPIJob() const
{
    xbt_assert(_profiles != nullptr, "Invalid Jobs::containsSMPIJob call: setProfiles had not been called yet");
    for (auto & mit : _jobs)
    {
        Job * job = mit.second;
        if ((*_profiles)[job->profile]->type == ProfileType::SMPI)
            return true;
    }
    return false;
}

void Jobs::displayDebug() const
{
    // Let us traverse jobs to display some information about them
    vector<string> jobsVector;
    for (auto & mit : _jobs)
    {
        jobsVector.push_back(std::to_string(mit.second->number));
    }

    // Let us create the string that will be sent to XBT_INFO
    string s = "Jobs debug information:\n";

    s += "There are " + to_string(_jobs.size()) + " jobs.\n";
    s += "Jobs : [" + boost::algorithm::join(jobsVector, ", ") + "]";

    // Let us display the string which has been built
    XBT_INFO("%s", s.c_str());
}

const std::map<int, Job* > &Jobs::jobs() const
{
    return _jobs;
}

bool job_comparator_subtime(const Job *a, const Job *b)
{
    return a->submission_time < b->submission_time;
}

Job * Job::from_json(const rapidjson::Value & json_desc, Workload * workload)
{
    Job * j = new Job;
    j->workload = workload;
    j->starting_time = -1;
    j->runtime = -1;
    j->state = JobState::JOB_STATE_NOT_SUBMITTED;
    j->consumed_energy = -1;
    string workload_name;

    xbt_assert(json_desc.IsObject(), "Invalid JSON: one job is not an object");

    xbt_assert(json_desc.HasMember("id"), "Invalid JSON: one job has no 'id' field");

    if (json_desc["id"].IsInt())
    {
        j->number = json_desc["id"].GetInt();
        workload_name = workload->name;
    }
    else if (json_desc["id"].IsString())
    {
        vector<string> job_identifier_parts;
        boost::split(job_identifier_parts, (const std::string) (json_desc["id"].GetString()),
                     boost::is_any_of("!"), boost::token_compress_on);
        workload_name = job_identifier_parts[0];
        XBT_INFO("========  %s : %s", workload->name.c_str(), workload_name.c_str());
        j->number = std::stoi(job_identifier_parts[1]);
    }
    else
    {
        XBT_ERROR("Invalid JSON: job %d id is neither a string nor an integer", j->number);
        xbt_abort();
    }

    xbt_assert(json_desc.HasMember("subtime"), "Invalid JSON: job %d has no 'subtime' field", j->number);
    xbt_assert(json_desc["subtime"].IsNumber(), "Invalid JSON: job %d has a non-number 'subtime' field", j->number);
    j->submission_time = json_desc["subtime"].GetDouble();

    xbt_assert(json_desc.HasMember("walltime"), "Invalid JSON: job %d has no 'walltime' field", j->number);
    xbt_assert(json_desc["walltime"].IsNumber(), "Invalid JSON: job %d has a non-number 'walltime' field", j->number);
    j->walltime = json_desc["walltime"].GetDouble();

    xbt_assert(json_desc.HasMember("res"), "Invalid JSON: job %d has no 'res' field", j->number);
    xbt_assert(json_desc["res"].IsInt(), "Invalid JSON: job %d has a non-number 'res' field", j->number);
    j->required_nb_res = json_desc["res"].GetInt();

    xbt_assert(json_desc.HasMember("profile"), "Invalid JSON: job %d has no 'profile' field", j->number);
    xbt_assert(json_desc["profile"].IsString(), "Invalid JSON: job %d has a non-string 'profile' field", j->number);
    j->profile = json_desc["profile"].GetString();

    // Let's get the JSON string which originally described the job (to conserve potential fields unused by Batsim)
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_desc.Accept(writer);

    // Let's replace the job ID by its WLOAD!NUMBER counterpart
    string json_description_tmp = buffer.GetString();
    boost::regex r("\"id\"\\s*:\\s*(?:\".+\"|\\d+)\\s*,");
    string replacement_str = "\"id\":\"" + workload_name + "!" + std::to_string(j->number) + "\",";
    j->json_description = boost::regex_replace(json_description_tmp, r, replacement_str);




    XBT_INFO("Loaded job %d from workload %s. The remainder of this string is just here to explicit the Black Hole issue (#1).", (int) j->number, j->workload->name.c_str() );

    return j;
}

Job * Job::from_json(const std::string & json_str, Workload *workload)
{
    Document doc;
    doc.Parse(json_str.c_str());

    return Job::from_json(doc, workload);
}

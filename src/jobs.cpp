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

#include <simgrid/s4u.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "profiles.hpp"

using namespace std;
using namespace rapidjson;

XBT_LOG_NEW_DEFAULT_CATEGORY(jobs, "jobs"); //!< Logging

JobIdentifier::JobIdentifier(const std::string & workload_name,
                             const std::string & job_name) :
    _workload_name(workload_name),
    _job_name(job_name)
{
    check_lexically_valid();
    _representation = representation();
}

JobIdentifier::JobIdentifier(const std::string & job_id_str)
{
    // Split the job_identifier by '!'
    vector<string> job_identifier_parts;
    boost::split(job_identifier_parts, job_id_str,
                 boost::is_any_of("!"), boost::token_compress_on);

    xbt_assert(job_identifier_parts.size() == 2,
               "Invalid string job identifier '%s': should be formatted as two '!'-separated "
               "parts, the second one being any string without '!'. Example: 'some_text!42'.",
               job_id_str.c_str());

    this->_workload_name = job_identifier_parts[0];
    this->_job_name = job_identifier_parts[1];

    check_lexically_valid();
    _representation = representation();
}

std::string JobIdentifier::to_string() const
{
    return _representation;
}

const char *JobIdentifier::to_cstring() const
{
    return _representation.c_str();
}

bool JobIdentifier::is_lexically_valid(std::string & reason) const
{
    bool ret = true;
    reason.clear();

    if(_workload_name.find('!') != std::string::npos)
    {
        ret = false;
        reason += "Invalid workload_name '" + _workload_name + "': contains a '!'.";
    }

    if(_job_name.find('!') != std::string::npos)
    {
        ret = false;
        reason += "Invalid job_name '" + _job_name + "': contains a '!'.";
    }

    return ret;
}

void JobIdentifier::check_lexically_valid() const
{
    string reason;
    xbt_assert(is_lexically_valid(reason), "%s", reason.c_str());
}

string JobIdentifier::workload_name() const
{
    return _workload_name;
}

string JobIdentifier::job_name() const
{
    return _job_name;
}

string JobIdentifier::representation() const
{
    return _workload_name + '!' + _job_name;
}

bool operator<(const JobIdentifier &ji1, const JobIdentifier &ji2)
{
    return ji1.to_string() < ji2.to_string();
}

bool operator==(const JobIdentifier &ji1, const JobIdentifier &ji2)
{
    return ji1.to_string() == ji2.to_string();
}

std::size_t JobIdentifierHasher::operator()(const JobIdentifier & id) const
{
    return std::hash<std::string>()(id.to_string());
}


BatTask::BatTask(JobPtr parent_job, ProfilePtr profile) :
    parent_job(parent_job),
    profile(profile)
{
}

BatTask::~BatTask()
{
    for (auto & sub_btask : this->sub_tasks)
    {
        delete sub_btask;
        sub_btask = nullptr;
    }
}

void BatTask::compute_leaf_progress()
{
    xbt_assert(sub_tasks.empty(), "Leaves should not contain sub tasks");

    if (profile->is_parallel_task())
    {
        if (ptask != nullptr) // The parallel task has already started
        {
            // WARNING: 'get_remaining_ratio' is not returning the flops amount but the remaining quantity of work
            // from 1 (not started yet) to 0 (completely finished)
            current_task_progress_ratio = 1 - ptask->get_remaining_ratio();
        }
        else
        {
            current_task_progress_ratio = 0;
        }
    }
    else if (profile->type == ProfileType::DELAY)
    {
        xbt_assert(delay_task_start != -1, "Internal error");

        double runtime = simgrid::s4u::Engine::get_clock() - delay_task_start;

        // Manages empty delay job (TODO why?!)
        if (delay_task_required == 0)
        {
            current_task_progress_ratio = 1;
        }
        else
        {
            current_task_progress_ratio = runtime / delay_task_required;
        }
    }
    else
    {
        XBT_WARN("Computing the progress of %s profiles is not implemented.",
                 profile_type_to_string(profile->type).c_str());
    }
}

void BatTask::compute_tasks_progress()
{
    if (profile->type == ProfileType::SEQUENCE)
    {
        sub_tasks[current_task_index]->compute_tasks_progress();
    }
    else
    {
        this->compute_leaf_progress();
    }
}

BatTask* Job::compute_job_progress()
{
    xbt_assert(task != nullptr, "Internal error");

    task->compute_tasks_progress();
    return task;
}



Jobs::~Jobs()
{
    _jobs.clear();
}

void Jobs::set_profiles(Profiles *profiles)
{
    _profiles = profiles;
}

void Jobs::set_workload(Workload *workload)
{
    _workload = workload;
}

void Jobs::load_from_json(const rapidjson::Document &doc, const std::string &filename)
{
    string error_prefix = "Invalid JSON file '" + filename + "'";

    xbt_assert(doc.IsObject(), "%s: not a JSON object", error_prefix.c_str());
    xbt_assert(doc.HasMember("jobs"), "%s: the 'jobs' array is missing", error_prefix.c_str());
    const Value & jobs = doc["jobs"];
    xbt_assert(jobs.IsArray(), "%s: the 'jobs' member is not an array", error_prefix.c_str());

    for (SizeType i = 0; i < jobs.Size(); i++) // Uses SizeType instead of size_t
    {
        const Value & job_json_description = jobs[i];

        auto j = Job::from_json(job_json_description, _workload, error_prefix);

        xbt_assert(!exists(j->id), "%s: duplication of job id '%s'",
                   error_prefix.c_str(), j->id.to_string().c_str());
        _jobs[j->id] = j;
        _jobs_met.insert({j->id, true});
    }
}

JobPtr Jobs::operator[](JobIdentifier job_id)
{
    auto it = _jobs.find(job_id);
    xbt_assert(it != _jobs.end(), "Cannot get job '%s': it does not exist",
               job_id.to_cstring());
    return it->second;
}

const JobPtr Jobs::operator[](JobIdentifier job_id) const
{
    auto it = _jobs.find(job_id);
    xbt_assert(it != _jobs.end(), "Cannot get job '%s': it does not exist",
               job_id.to_cstring());
    return it->second;
}

JobPtr Jobs::at(JobIdentifier job_id)
{
    return operator[](job_id);
}

const JobPtr Jobs::at(JobIdentifier job_id) const
{
    return operator[](job_id);
}

void Jobs::add_job(JobPtr job)
{
    xbt_assert(!exists(job->id),
               "Bad Jobs::add_job call: A job with name='%s' already exists.",
               job->id.to_cstring());

    _jobs[job->id] = job;
    _jobs_met.insert({job->id, true});
}

void Jobs::delete_job(const JobIdentifier & job_id, const bool & garbage_collect_profiles)
{
    xbt_assert(exists(job_id),
               "Bad Jobs::delete_job call: The job with name='%s' does not exist.",
               job_id.to_cstring());

    std::string profile_name = _jobs[job_id]->profile->name;
    _jobs.erase(job_id);
    if (garbage_collect_profiles)
    {
        _workload->profiles->remove_profile(profile_name);
    }
}

bool Jobs::exists(const JobIdentifier & job_id) const
{
    auto it = _jobs_met.find(job_id);
    return it != _jobs_met.end();
}

bool Jobs::contains_smpi_job() const
{
    xbt_assert(_profiles != nullptr, "Invalid Jobs::containsSMPIJob call: setProfiles had not been called yet");
    for (auto & mit : _jobs)
    {
        auto job = mit.second;
        if (job->profile->type == ProfileType::SMPI)
        {
            return true;
        }
    }
    return false;
}

void Jobs::displayDebug() const
{
    // Let us traverse jobs to display some information about them
    vector<string> jobsVector;
    for (auto & mit : _jobs)
    {
        jobsVector.push_back(mit.second->id.to_string());
    }

    // Let us create the string that will be sent to XBT_INFO
    string s = "Jobs debug information:\n";

    s += "There are " + to_string(_jobs.size()) + " jobs.\n";
    s += "Jobs : [" + boost::algorithm::join(jobsVector, ", ") + "]";

    // Let us display the string which has been built
    XBT_DEBUG("%s", s.c_str());
}

const std::unordered_map<JobIdentifier, JobPtr, JobIdentifierHasher> &Jobs::jobs() const
{
    return _jobs;
}

std::unordered_map<JobIdentifier, JobPtr, JobIdentifierHasher> &Jobs::jobs()
{
    return _jobs;
}

int Jobs::nb_jobs() const
{
    return _jobs.size();
}

bool job_comparator_subtime_number(const JobPtr a, const JobPtr b)
{
    if (a->submission_time == b->submission_time)
    {
        return a->id.to_string() < b->id.to_string();
    }
    return a->submission_time < b->submission_time;
}

Job::~Job()
{
    XBT_INFO("Job '%s' is being deleted", id.to_string().c_str());
    xbt_assert(execution_actors.size() == 0,
               "Internal error: job %s on destruction still has %d execution processes (should be 0).",
               this->id.to_string().c_str(), (int)execution_actors.size());

    if (task != nullptr)
    {
        delete task;
        task = nullptr;
    }
}

bool operator<(const Job &j1, const Job &j2)
{
    return j1.id < j2.id;
}

bool Job::is_complete() const
{
    return (state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY) ||
           (state == JobState::JOB_STATE_COMPLETED_KILLED) ||
           (state == JobState::JOB_STATE_COMPLETED_FAILED) ||
           (state == JobState::JOB_STATE_REJECTED) ||
           (state == JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED);
}

// Do NOT remove namespaces in the arguments (to avoid doxygen warnings)
JobPtr Job::from_json(const rapidjson::Value & json_desc,
                     Workload * workload,
                     const std::string & error_prefix)
{
    // Create and initialize with default values
    auto j = std::make_shared<Job>();
    j->workload = workload;
    j->starting_time = -1;
    j->runtime = -1;
    j->state = JobState::JOB_STATE_NOT_SUBMITTED;
    j->consumed_energy = -1;

    xbt_assert(json_desc.IsObject(), "%s: one job is not an object", error_prefix.c_str());

    // Get job id and create a JobIdentifier
    xbt_assert(json_desc.HasMember("id"), "%s: one job has no 'id' field", error_prefix.c_str());
    xbt_assert(json_desc["id"].IsString() or json_desc["id"].IsInt(), "%s: on job id field is invalid, it should be a string or an integer", error_prefix.c_str());
    string job_id_str;
    if (json_desc["id"].IsString())
    {
        job_id_str = json_desc["id"].GetString();
    }
    else if (json_desc["id"].IsInt())
    {
        job_id_str = to_string(json_desc["id"].GetInt());
    }

    if (job_id_str.find(workload->name) == std::string::npos)
    {
        // the workload name is not present in the job id string
        j->id = JobIdentifier(workload->name, job_id_str);
    }
    else
    {
        j->id = JobIdentifier(job_id_str);
    }

    // Get submission time
    xbt_assert(json_desc.HasMember("subtime"), "%s: job '%s' has no 'subtime' field",
               error_prefix.c_str(), j->id.to_string().c_str());
    xbt_assert(json_desc["subtime"].IsNumber(), "%s: job '%s' has a non-number 'subtime' field",
               error_prefix.c_str(), j->id.to_string().c_str());
    j->submission_time = json_desc["subtime"].GetDouble();

    // Get walltime (optional)
    if (!json_desc.HasMember("walltime"))
    {
        XBT_INFO("job '%s' has no 'walltime' field", j->id.to_string().c_str());
    }
    else
    {
        xbt_assert(json_desc["walltime"].IsNumber(), "%s: job %s has a non-number 'walltime' field",
                   error_prefix.c_str(), j->id.to_string().c_str());
        j->walltime = json_desc["walltime"].GetDouble();
    }
    xbt_assert(j->walltime == -1 || j->walltime > 0,
               "%s: job '%s' has an invalid walltime (%g). It should either be -1 (no walltime) "
               "or a strictly positive number.",
               error_prefix.c_str(), j->id.to_string().c_str(), (double)j->walltime);

    // Get number of requested resources
    xbt_assert(json_desc.HasMember("res"), "%s: job %s has no 'res' field",
               error_prefix.c_str(), j->id.to_string().c_str());
    xbt_assert(json_desc["res"].IsInt(), "%s: job %s has a non-number 'res' field",
               error_prefix.c_str(), j->id.to_string().c_str());
    j->requested_nb_res = json_desc["res"].GetInt();

    // Get the job profile
    xbt_assert(json_desc.HasMember("profile"), "%s: job %s has no 'profile' field",
               error_prefix.c_str(), j->id.to_string().c_str());
    xbt_assert(json_desc["profile"].IsString(), "%s: job %s has a non-string 'profile' field",
               error_prefix.c_str(), j->id.to_string().c_str());

    // TODO raise exception when the profile does not exist.
    std::string profile_name = json_desc["profile"].GetString();
    xbt_assert(workload->profiles->exists(profile_name), "%s: the profile %s for job %s does not exist",
               error_prefix.c_str(), profile_name.c_str(), j->id.to_string().c_str());
    j->profile = workload->profiles->at(profile_name);

    XBT_INFO("Profile name %s and '%s'", profile_name.c_str(), j->profile->name.c_str());

    // Let's get the JSON string which originally described the job
    // (to conserve potential fields unused by Batsim)
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_desc.Accept(writer);

    // Let's replace the job ID by its WLOAD!NUMBER counterpart if needed
    // in the json raw description
    string json_description_tmp(buffer.GetString(), buffer.GetSize());
    /// @cond DOXYGEN_FAILS_PARSING_THIS_REGEX
    boost::regex r(R"("id"\s*:\s*(?:"*[^(,|})]*"*)\s*)");
    /// @endcond
    string replacement_str = "\"id\":\"" + j->id.to_string() + "\"";
    // XBT_INFO("Before regexp: %s", json_description_tmp.c_str());
    j->json_description = boost::regex_replace(json_description_tmp, r, replacement_str);

    // Let's check that the new description is a valid JSON string
    rapidjson::Document check_doc;
    check_doc.Parse(j->json_description.c_str());
    xbt_assert(!check_doc.HasParseError(),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart:"
               "The output string '%s' is not valid JSON.", j->json_description.c_str());
    xbt_assert(check_doc.IsObject(),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output string '%s' is not valid JSON.", j->json_description.c_str());
    xbt_assert(check_doc.HasMember("id"),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output JSON '%s' has no 'id' field.", j->json_description.c_str());
    xbt_assert(check_doc["id"].IsString(),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output JSON '%s' has a non-string 'id' field.", j->json_description.c_str());
    xbt_assert(check_doc.HasMember("subtime") && check_doc["subtime"].IsNumber(),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output JSON '%s' has no 'subtime' field (or it is not a number)",
               j->json_description.c_str());
    xbt_assert((check_doc.HasMember("walltime") && check_doc["walltime"].IsNumber())
               || (!check_doc.HasMember("walltime")),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output JSON '%s' has no 'walltime' field (or it is not a number)",
               j->json_description.c_str());
    xbt_assert(check_doc.HasMember("res") && check_doc["res"].IsInt(),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output JSON '%s' has no 'res' field (or it is not an integer)",
               j->json_description.c_str());
    xbt_assert(check_doc.HasMember("profile") && check_doc["profile"].IsString(),
               "A problem occured when replacing the job_id by its WLOAD!job_name counterpart: "
               "The output JSON '%s' has no 'profile' field (or it is not a string)",
               j->json_description.c_str());

    if (json_desc.HasMember("smpi_ranks_to_hosts_mapping"))
    {
        xbt_assert(json_desc["smpi_ranks_to_hosts_mapping"].IsArray(),
                "%s: job '%s' has a non-array 'smpi_ranks_to_hosts_mapping' field",
                error_prefix.c_str(), j->id.to_string().c_str());

        const auto & mapping_array = json_desc["smpi_ranks_to_hosts_mapping"];
        j->smpi_ranks_to_hosts_mapping.resize(mapping_array.Size());

        for (unsigned int i = 0; i < mapping_array.Size(); ++i)
        {
            xbt_assert(mapping_array[i].IsInt(),
                       "%s: job '%s' has a bad 'smpi_ranks_to_hosts_mapping' field: rank "
                       "%d does not point to an integral number",
                       error_prefix.c_str(), j->id.to_string().c_str(), i);
            int host_number = mapping_array[i].GetInt();
            xbt_assert(host_number >= 0 && (unsigned int)host_number < j->requested_nb_res,
                       "%s: job '%s' has a bad 'smpi_ranks_to_hosts_mapping' field: rank "
                       "%d has an invalid value %d : should be in [0,%d[",
                       error_prefix.c_str(), j->id.to_string().c_str(),
                       i, host_number, j->requested_nb_res);

            j->smpi_ranks_to_hosts_mapping[i] = host_number;
        }
    }

    XBT_DEBUG("Job '%s' Loaded", j->id.to_string().c_str());
    return j;
}

// Do NOT remove namespaces in the arguments (to avoid doxygen warnings)
JobPtr Job::from_json(const std::string & json_str,
                     Workload * workload,
                     const std::string & error_prefix)
{
    Document doc;
    doc.Parse(json_str.c_str());
    xbt_assert(!doc.HasParseError(),
               "%s: Cannot be parsed. Content (between '##'):\n#%s#",
               error_prefix.c_str(), json_str.c_str());

    return Job::from_json(doc, workload, error_prefix);
}

std::string job_state_to_string(const JobState & state)
{
    string job_state("UNKNOWN");

    switch (state)
    {
    case JobState::JOB_STATE_NOT_SUBMITTED:
        job_state = "NOT_SUBMITTED";
        break;
    case JobState::JOB_STATE_SUBMITTED:
        job_state = "SUBMITTED";
        break;
    case JobState::JOB_STATE_RUNNING:
        job_state = "RUNNING";
        break;
    case JobState::JOB_STATE_COMPLETED_SUCCESSFULLY:
        job_state = "COMPLETED_SUCCESSFULLY";
        break;
    case JobState::JOB_STATE_COMPLETED_FAILED:
        job_state = "COMPLETED_FAILED";
        break;
    case JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED:
        job_state = "COMPLETED_WALLTIME_REACHED";
        break;
    case JobState::JOB_STATE_COMPLETED_KILLED:
        job_state = "COMPLETED_KILLED";
        break;
    case JobState::JOB_STATE_REJECTED:
        job_state = "REJECTED";
        break;
    }
    return job_state;
}

JobState job_state_from_string(const std::string & state)
{
    JobState new_state;

    if (state == "NOT_SUBMITTED")
    {
        new_state = JobState::JOB_STATE_NOT_SUBMITTED;
    }
    else if (state == "SUBMITTED")
    {
        new_state = JobState::JOB_STATE_SUBMITTED;
    }
    else if (state == "RUNNING")
    {
        new_state = JobState::JOB_STATE_RUNNING;
    }
    else if (state == "COMPLETED_SUCCESSFULLY")
    {
        new_state = JobState::JOB_STATE_COMPLETED_SUCCESSFULLY;
    }
    else if (state == "COMPLETED_FAILED")
    {
        new_state = JobState::JOB_STATE_COMPLETED_FAILED;
    }
    else if (state == "COMPLETED_KILLED")
    {
        new_state = JobState::JOB_STATE_COMPLETED_KILLED;
    }
    else if (state == "COMPLETED_WALLTIME_REACHED")
    {
        new_state = JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED;
    }
    else if (state == "REJECTED")
    {
        new_state = JobState::JOB_STATE_REJECTED;
    }
    else
    {
        xbt_assert(false, "Invalid job state");
    }

    return new_state;
}

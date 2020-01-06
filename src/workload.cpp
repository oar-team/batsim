/**
 * @file workload.cpp
 * @brief Contains workload-related functions
 */

#include "workload.hpp"

#include <fstream>
#include <streambuf>

#include <rapidjson/document.h>

#include <smpi/smpi.h>

#include "context.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "jobs_execution.hpp"

using namespace std;
using namespace rapidjson;

XBT_LOG_NEW_DEFAULT_CATEGORY(workload, "workload"); //!< Logging

Workload *Workload::new_static_workload(const string & workload_name,
                                        const string & workload_file)
{
    Workload * workload = new Workload;

    workload->jobs = new Jobs;
    workload->profiles = new Profiles;

    workload->jobs->set_profiles(workload->profiles);
    workload->jobs->set_workload(workload);
    workload->name = workload_name;
    workload->file = workload_file;

    workload->_is_static = true;
    return workload;
}

Workload *Workload::new_dynamic_workload(const string & workload_name)
{
    Workload * workload = new_static_workload(workload_name, "dynamic");

    workload->_is_static = false;
    return workload;
}


Workload::~Workload()
{
    delete jobs;
    delete profiles;

    jobs = nullptr;
    profiles = nullptr;
}

void Workload::load_from_json(const std::string &json_filename, int &nb_machines)
{
    XBT_INFO("Loading JSON workload '%s'...", json_filename.c_str());
    // Let the file content be placed in a string
    ifstream ifile(json_filename);
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", json_filename.c_str());
    string content;

    ifile.seekg(0, ios::end);
    content.reserve(ifile.tellg());
    ifile.seekg(0, ios::beg);

    content.assign((std::istreambuf_iterator<char>(ifile)),
                std::istreambuf_iterator<char>());

    // JSON document creation
    Document doc;
    doc.Parse(content.c_str());
    xbt_assert(!doc.HasParseError(), "Invalid JSON file '%s': could not be parsed", json_filename.c_str());
    xbt_assert(doc.IsObject(), "Invalid JSON file '%s': not a JSON object", json_filename.c_str());

    // Let's try to read the number of machines in the JSON document
    xbt_assert(doc.HasMember("nb_res"), "Invalid JSON file '%s': the 'nb_res' field is missing", json_filename.c_str());
    const Value & nb_res_node = doc["nb_res"];
    xbt_assert(nb_res_node.IsInt(), "Invalid JSON file '%s': the 'nb_res' field is not an integer", json_filename.c_str());
    nb_machines = nb_res_node.GetInt();
    xbt_assert(nb_machines > 0, "Invalid JSON file '%s': the value of the 'nb_res' field is invalid (%d)",
               json_filename.c_str(), nb_machines);

    profiles->load_from_json(doc, json_filename);
    jobs->load_from_json(doc, json_filename);

    XBT_INFO("JSON workload parsed sucessfully. Read %d jobs and %d profiles.",
             jobs->nb_jobs(), profiles->nb_profiles());
    XBT_INFO("Checking workload validity...");
    check_validity();
    XBT_INFO("Workload seems to be valid.");
}

void Workload::register_smpi_applications()
{
    XBT_INFO("Registering SMPI applications of workload '%s'...", name.c_str());

    for (auto mit : jobs->jobs())
    {
        auto job = mit.second;

        if (job->profile->type == ProfileType::SMPI)
        {
            SmpiProfileData * data = (SmpiProfileData *) job->profile->data;

            XBT_INFO("Registering app. instance='%s', nb_process=%d",
                     job->id.to_string().c_str(), (int) data->trace_filenames.size());
            SMPI_app_instance_register(job->id.to_string().c_str(), nullptr, data->trace_filenames.size());
        }
    }

    XBT_INFO("SMPI applications of workload '%s' have been registered.", name.c_str());
}

void Workload::check_validity()
{
    // Let's check that every SEQUENCE-typed profile points to existing profiles
    // And update the refcounting of these profiles
    for (auto mit : profiles->profiles())
    {
        auto profile = mit.second;
        if (profile->type == ProfileType::SEQUENCE)
        {
            SequenceProfileData * data = (SequenceProfileData *) profile->data;
            data->profile_sequence.reserve(data->sequence.size());
            for (const auto & prof : data->sequence)
            {
                (void) prof; // Avoids a warning if assertions are ignored
                xbt_assert(profiles->exists(prof),
                           "Invalid composed profile '%s': the used profile '%s' does not exist",
                           mit.first.c_str(), prof.c_str());
                // Adds one to the refcounting for the profile 'prof'
                data->profile_sequence.push_back(profiles->at(prof));
            }
        }
    }

    // TODO : check that there are no circular calls between composed profiles...
    // TODO: compute the constraint of the profile number of resources, to check if it matches the jobs that use it

    // Let's check the profile validity of each job
    for (auto mit : jobs->jobs())
    {
        check_single_job_validity(mit.second);
    }
}

void Workload::check_single_job_validity(const JobPtr job)
{
    //TODO This is already checked during creation of the job in Job::from_json
    xbt_assert(profiles->exists(job->profile->name),
               "Invalid job %s: the associated profile '%s' does not exist",
               job->id.to_string().c_str(), job->profile->name.c_str());

    if (job->profile->type == ProfileType::PARALLEL)
    {
        ParallelProfileData * data = (ParallelProfileData *) job->profile->data;
        (void) data; // Avoids a warning if assertions are ignored
        xbt_assert(data->nb_res == job->requested_nb_res,
                   "Invalid job %s: the requested number of resources (%d) do NOT match"
                   " the number of resources of the associated profile '%s' (%d)",
                   job->id.to_string().c_str(), job->requested_nb_res, job->profile->name.c_str(), data->nb_res);
    }
    /*else if (job->profile->type == ProfileType::SEQUENCE)
    {
        // TODO: check if the number of resources matches a resource-constrained composed profile
    }*/
}

string Workload::to_string()
{
    return this->name;
}

bool Workload::is_static() const
{
    return _is_static;
}

Workloads::~Workloads()
{
    for (auto mit : _workloads)
    {
        Workload * workload = mit.second;
        delete workload;
    }
    _workloads.clear();
}

Workload *Workloads::operator[](const std::string &workload_name)
{
    return at(workload_name);
}

const Workload *Workloads::operator[](const std::string &workload_name) const
{
    return at(workload_name);
}

Workload *Workloads::at(const std::string &workload_name)
{
    return _workloads.at(workload_name);
}

const Workload *Workloads::at(const std::string &workload_name) const
{
    return _workloads.at(workload_name);
}

int Workloads::nb_workloads() const
{
    return _workloads.size();
}

int Workloads::nb_static_workloads() const
{
    int count = 0;

    for (auto mit : _workloads)
    {
        Workload * workload = mit.second;

        count += int(workload->is_static());
    }

    return count;
}

JobPtr Workloads::job_at(const JobIdentifier &job_id)
{
    return at(job_id.workload_name)->jobs->at(job_id);
}

const JobPtr Workloads::job_at(const JobIdentifier &job_id) const
{
    return at(job_id.workload_name)->jobs->at(job_id);
}

void Workloads::delete_jobs(const vector<JobIdentifier> & job_ids,
                            const bool & garbage_collect_profiles)
{
    for (const JobIdentifier & job_id : job_ids)
    {
        at(job_id.workload_name)->jobs->delete_job(job_id, garbage_collect_profiles);
    }
}

void Workloads::insert_workload(const std::string &workload_name, Workload *workload)
{
    xbt_assert(!exists(workload_name));
    xbt_assert(!exists(workload->name));

    workload->name = workload_name;
    _workloads[workload_name] = workload;
}

bool Workloads::exists(const std::string &workload_name) const
{
    return _workloads.count(workload_name) == 1;
}

bool Workloads::contains_smpi_job() const
{
    for (auto mit : _workloads)
    {
        Workload * workload = mit.second;
        if (workload->jobs->contains_smpi_job())
        {
            return true;
        }
    }

    return false;
}

void Workloads::register_smpi_applications()
{
    for (auto mit : _workloads)
    {
        Workload * workload = mit.second;
        workload->register_smpi_applications();
    }
}

bool Workloads::job_is_registered(const JobIdentifier &job_id)
{
    at(job_id.workload_name)->jobs->displayDebug();
    return at(job_id.workload_name)->jobs->exists(job_id);
}

bool Workloads::job_profile_is_registered(const JobIdentifier &job_id)
{
    //TODO this could be improved/simplified
    auto job = at(job_id.workload_name)->jobs->at(job_id);
    return at(job_id.workload_name)->profiles->exists(job->profile->name);
}

std::map<std::string, Workload *> &Workloads::workloads()
{
    return _workloads;
}

const std::map<std::string, Workload *> &Workloads::workloads() const
{
    return _workloads;
}

string Workloads::to_string()
{
    string str;
    for (auto mit : _workloads)
    {
        string key = mit.first;
        Workload * workload = mit.second;
        str += workload->to_string() + " ";
    }
    return str;
}

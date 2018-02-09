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

Workload::Workload(const std::string & workload_name, const std::string & workload_file)
{
    jobs = new Jobs;
    profiles = new Profiles;

    jobs->set_profiles(profiles);
    jobs->set_workload(this);
    this->name = workload_name;
    this->file = workload_file;
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

    jobs->load_from_json(doc, json_filename);
    profiles->load_from_json(doc, json_filename);

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
        Job * job = mit.second;
        Profile * profile = (*profiles)[job->profile];

        if (profile->type == ProfileType::SMPI)
        {
            SmpiProfileData * data = (SmpiProfileData *) profile->data;

            XBT_INFO("Registering app. instance='%s', nb_process=%d",
                     job->id.to_string().c_str(), (int) data->trace_filenames.size());
            SMPI_app_instance_register(job->_id_str.c_str(), smpi_replay_process, data->trace_filenames.size());
        }
    }

    XBT_INFO("SMPI applications of workload '%s' have been registered.", name.c_str());
}

void Workload::check_validity()
{
    // Let's check that every SEQUENCE-typed profile points to existing profiles
    for (auto mit : profiles->profiles())
    {
        Profile * profile = mit.second;
        if (profile->type == ProfileType::SEQUENCE)
        {
            SequenceProfileData * data = (SequenceProfileData *) profile->data;
            for (const auto & prof : data->sequence)
            {
                (void) prof; // Avoids a warning if assertions are ignored
                xbt_assert(profiles->exists(prof),
                           "Invalid composed profile '%s': the used profile '%s' does not exist",
                           mit.first.c_str(), prof.c_str());
            }
        }
    }

    // TODO : check that there are no circular calls between composed profiles...
    // TODO: compute the constraint of the profile number of resources, to check if it match the jobs that use it

    // Let's check that the profile of each job exists
    for (auto mit : jobs->jobs())
    {
        Job * job = mit.second;
        xbt_assert(profiles->exists(job->profile),
                   "Invalid job %s: the associated profile '%s' does not exist",
                   job->id.to_string().c_str(), job->profile.c_str());

        const Profile * profile = profiles->at(job->profile);
        if (profile->type == ProfileType::MSG_PARALLEL)
        {
            MsgParallelProfileData * data = (MsgParallelProfileData *) profile->data;
            (void) data; // Avoids a warning if assertions are ignored
            xbt_assert(data->nb_res == job->requested_nb_res,
                       "Invalid job %s: the requested number of resources (%d) do NOT match"
                       " the number of resources of the associated profile '%s' (%d)",
                       job->id.to_string().c_str(), job->requested_nb_res, job->profile.c_str(), data->nb_res);
        }
        else if (profile->type == ProfileType::SEQUENCE)
        {
            // TODO: check if the number of resources matches a resource-constrained composed profile
        }
    }
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
    xbt_assert(exists(workload_name));
    return _workloads.at(workload_name);
}

const Workload *Workloads::at(const std::string &workload_name) const
{
    xbt_assert(exists(workload_name));
    return _workloads.at(workload_name);
}

int Workloads::nb_workloads() const
{
    return _workloads.size();
}

Job *Workloads::job_at(const std::string &workload_name, int job_number)
{
    return at(workload_name)->jobs->at(job_number);
}

const Job *Workloads::job_at(const std::string &workload_name, int job_number) const
{
    return at(workload_name)->jobs->at(job_number);
}

Job *Workloads::job_at(const JobIdentifier &job_id)
{
    return at(job_id.workload_name)->jobs->at(job_id.job_name);
}

const Job *Workloads::job_at(const JobIdentifier &job_id) const
{
    return at(job_id.workload_name)->jobs->at(job_id.job_name);
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

bool Workloads::job_is_registered(const std::string &workload_name, const int job_number)
{
    return at(workload_name)->jobs->exists(job_number);
}

bool Workloads::job_profile_is_registered(const std::string &workload_name, const int job_number)
{
    const Job * job = at(workload_name)->jobs->at(job_number);
    return at(workload_name)->profiles->exists(job->profile);
}

bool Workloads::job_is_registered(const JobIdentifier &job_id)
{
    return job_is_registered(job_id.workload_name, job_id.job_name);
}

bool Workloads::job_profile_is_registered(const JobIdentifier &job_id)
{
    return job_profile_is_registered(job_id.workload_name, job_id.job_name);
}

std::map<std::string, Workload *> &Workloads::workloads()
{
    return _workloads;
}

const std::map<std::string, Workload *> &Workloads::workloads() const
{
    return _workloads;
}

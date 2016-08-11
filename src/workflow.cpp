/**
 * @file workflow.cpp
 * @brief Contains workflow-related functions
 */

#include "workflow.hpp"

#include <fstream>
#include <streambuf>

// TODO : include xml lib
//#include <rapidjson/document.h>

#include <smpi/smpi.h>

#include "context.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "jobs_execution.hpp"

using namespace std;
// TODO : include xml namespace
//using namespace rapidjson;

XBT_LOG_NEW_DEFAULT_CATEGORY(workflow, "workflow"); //!< Logging

Workflow::Workflow()
{
    jobs = new Jobs;
    profiles = new Profiles;

    jobs->setProfiles(profiles);
    jobs->setWorkflow(this);
}

Workflow::~Workflow()
{
    delete jobs;
    delete profiles;

    jobs = nullptr;
    profiles = nullptr;
}

void Workflow::load_from_xml(const std::string &xml_filename, int &nb_machines)
{
    XBT_INFO("Loading XML workflow '%s'...", xml_filename.c_str());
    // Let the file content be placed in a string
    ifstream ifile(xml_filename);
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", xml_filename.c_str());
    string content;

    ifile.seekg(0, ios::end);
    content.reserve(ifile.tellg());
    ifile.seekg(0, ios::beg);

    content.assign((std::istreambuf_iterator<char>(ifile)),
                std::istreambuf_iterator<char>());

    // XML document creation
    Document doc;
    doc.Parse(content.c_str());
    xbt_assert(doc.IsObject());

    // Let's try to read the number of machines in the XML document
    xbt_assert(doc.HasMember("nb_res"), "Invalid XML file '%s': the 'nb_res' field is missing", xml_filename.c_str());
    const Value & nb_res_node = doc["nb_res"];
    xbt_assert(nb_res_node.IsInt(), "Invalid XML file '%s': the 'nb_res' field is not an integer", xml_filename.c_str());
    nb_machines = nb_res_node.GetInt();
    xbt_assert(nb_machines > 0, "Invalid XML file '%s': the value of the 'nb_res' field is invalid (%d)",
               xml_filename.c_str(), nb_machines);

    jobs->load_from_xml(doc, xml_filename);
    profiles->load_from_xml(doc, xml_filename);

    XBT_INFO("XML workflow parsed sucessfully.");
    XBT_INFO("Checking workflow validity...");
    check_validity();
    XBT_INFO("Workflow seems to be valid.");
}

void Workflow::register_smpi_applications()
{
    XBT_INFO("Registering SMPI applications...");
    for (auto mit : jobs->jobs())
    {
        Job * job = mit.second;
        string job_id_str = to_string(job->number);
        XBT_INFO("Registering app. instance='%s', nb_process=%d", job_id_str.c_str(), job->required_nb_res);
        SMPI_app_instance_register(job_id_str.c_str(), smpi_replay_process, job->required_nb_res);
    }
    XBT_INFO("SMPI applications have been registered");
}

void Workflow::check_validity()
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
                   "Invalid job %d: the associated profile '%s' does not exist",
                   job->number, job->profile.c_str());

        const Profile * profile = profiles->at(job->profile);
        if (profile->type == ProfileType::MSG_PARALLEL)
        {
            MsgParallelProfileData * data = (MsgParallelProfileData *) profile->data;
            (void) data; // Avoids a warning if assertions are ignored
            xbt_assert(data->nb_res == job->required_nb_res,
                       "Invalid job %d: the requested number of resources (%d) do NOT match"
                       " the number of resources of the associated profile '%s' (%d)",
                       job->number, job->required_nb_res, job->profile.c_str(), data->nb_res);
        }
        else if (profile->type == ProfileType::SEQUENCE)
        {
            // TODO: check if the number of resources matches a resource-constrained composed profile
        }
    }
}



Workflows::Workflows()
{

}

Workflows::~Workflows()
{
    for (auto mit : _workflows)
    {
        Workflow * workflow = mit.second;
        delete workflow;
    }
    _workflows.clear();
}

Workflow *Workflows::operator[](const std::string &workflow_name)
{
    return at(workflow_name);
}

const Workflow *Workflows::operator[](const std::string &workflow_name) const
{
    return at(workflow_name);
}

Workflow *Workflows::at(const std::string &workflow_name)
{
    xbt_assert(exists(workflow_name));
    return _workflows.at(workflow_name);
}

const Workflow *Workflows::at(const std::string &workflow_name) const
{
    xbt_assert(exists(workflow_name));
    return _workflows.at(workflow_name);
}

Job *Workflows::job_at(const std::string &workflow_name, int job_number)
{
    return at(workflow_name)->jobs->at(job_number);
}

const Job *Workflows::job_at(const std::string &workflow_name, int job_number) const
{
    return at(workflow_name)->jobs->at(job_number);
}

Job *Workflows::job_at(const JobIdentifier &job_id)
{
    return at(job_id.workflow_name)->jobs->at(job_id.job_number);
}

const Job *Workflows::job_at(const JobIdentifier &job_id) const
{
    return at(job_id.workflow_name)->jobs->at(job_id.job_number);
}

void Workflows::insert_workflow(const std::string &workflow_name, Workflow *workflow)
{
    xbt_assert(!exists(workflow_name));
    xbt_assert(!exists(workflow->name));

    workflow->name = workflow_name;
    _workflows[workflow_name] = workflow;
}

bool Workflows::exists(const std::string &workflow_name) const
{
    return _workflows.count(workflow_name) == 1;
}

bool Workflows::job_exists(const std::string &workflow_name, const int job_number)
{
    if (!exists(workflow_name))
        return false;

    if (!at(workflow_name)->jobs->exists(job_number))
        return false;

    const Job * job = at(workflow_name)->jobs->at(job_number);
    if (!at(workflow_name)->profiles->exists(job->profile))
        return false;

    return true;
}

bool Workflows::job_exists(const JobIdentifier &job_id)
{
    return job_exists(job_id.workflow_name, job_id.job_number);
}

Job *Workflows::add_job_if_not_exists(const JobIdentifier &job_id, BatsimContext *context)
{
    xbt_assert(this == &context->workflows,
               "Bad Workflows::add_job_if_not_exists call: The given context "
               "does not match the Workflows instance (this=%p, &context->workflows=%p",
               this, &context->workflows);

    // If the job already exists, let's just return it
    if (job_exists(job_id))
        return job_at(job_id);

    // Let's create a Workflow if needed
    Workflow * workflow = nullptr;
    if (!exists(job_id.workflow_name))
    {
        workflow = new Workflow;
        workflow->name = job_id.workflow_name;
        insert_workflow(job_id.workflow_name, workflow);
    }
    else
        workflow = at(job_id.workflow_name);

    // Let's retrieve the job information from the data storage
    string job_key = RedisStorage::job_key(job_id);
    string job_xml_description = context->storage.get(job_key);

    // Let's create a Job if needed
    Job * job = nullptr;
    if (!workflow->jobs->exists(job_id.job_number))
    {
        job = Job::from_xml(job_xml_description, workflow);
        xbt_assert(job_id.job_number == job->number,
                   "Cannot add dynamic job %s!%d: XML job number mismatch (%d)",
                   job_id.workflow_name.c_str(), job_id.job_number, job->number);

        // Let's insert the Job in the data structure
        workflow->jobs->add_job(job);
    }
    else
        job = workflow->jobs->at(job_id.job_number);

    // Let's retrieve the profile information from the data storage
    string profile_key = RedisStorage::profile_key(workflow->name, job->profile);
    string profile_xml_description = context->storage.get(profile_key);

    // Let's create a Profile if needed
    Profile * profile = nullptr;
    if (!workflow->profiles->exists(job->profile))
    {
        profile = Profile::from_xml(job->profile, profile_xml_description);

        // Let's insert the Profile in the data structure
        workflow->profiles->add_profile(job->profile, profile);
    }
    else
        profile = workflow->profiles->at(job->profile);

    // TODO: check job & profile consistency (nb_res, etc.)
    return job;
}

std::map<std::string, Workflow *> &Workflows::workflows()
{
    return _workflows;
}

const std::map<std::string, Workflow *> &Workflows::workflows() const
{
    return _workflows;
}

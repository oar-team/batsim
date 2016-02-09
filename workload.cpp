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

XBT_LOG_NEW_DEFAULT_CATEGORY(workload, "workload");

void check_worload_validity(BatsimContext *context)
{
    // Let's check that every SEQUENCE-typed profile points to existing profiles
    for (auto mit : context->profiles.profiles())
    {
        Profile * profile = mit.second;
        if (profile->type == ProfileType::SEQUENCE)
        {
            SequenceProfileData * data = (SequenceProfileData *) profile->data;
            for (const auto & prof : data->sequence)
                xbt_assert(context->profiles.exists(prof), "Invalid composed profile '%s': the used profile '%s' does not exist", mit.first.c_str(), prof.c_str());
        }
    }

    // TODO : check that there are no circular calls between composed profiles...
    // TODO: compute the constraint of the profile number of resources, to check if it match the jobs that use it

    // Let's check that the profile of each job exists
    for (auto mit : context->jobs.jobs())
    {
        Job * job = mit.second;
        xbt_assert(context->profiles.exists(job->profile), "Invalid job %d: the associated profile '%s' does not exist", job->id, job->profile.c_str());

        Profile * profile = context->profiles[job->profile];
        if (profile->type == ProfileType::MSG_PARALLEL)
        {
            MsgParallelProfileData * data = (MsgParallelProfileData *) profile->data;
            xbt_assert(data->nb_res == job->required_nb_res, "Invalid job %d: the requested number of resources (%d) do NOT match"
                       " the number of resources of the associated profile '%s' (%d)", job->id, job->required_nb_res, job->profile.c_str(), data->nb_res);
        }
        else if (profile->type == ProfileType::SEQUENCE)
        {
            // TODO: check if the number of resources matches a resource-constrained composed profile
        }

    }
}

void load_json_workload(BatsimContext *context, const std::string &filename)
{
    XBT_INFO("Loading JSON workload '%s'...", filename.c_str());
    // Let the file content be placed in a string
    ifstream ifile(filename);
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", filename.c_str());
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
    context->jobs.load_from_json(doc, filename);
    context->profiles.load_from_json(doc, filename);

    XBT_INFO("JSON workload parsed sucessfully.");
    XBT_INFO("Checking workload validity...");
    check_worload_validity(context);
    XBT_INFO("Workload seems to be valid.");
}

void register_smpi_applications(BatsimContext *context)
{
    XBT_INFO("Registering SMPI applications...");
    for (auto mit : context->jobs.jobs())
    {
        Job * job = mit.second;
        string job_id_str = to_string(job->id);
        XBT_INFO("Registering app. instance='%s', nb_process=%d", job_id_str.c_str(), job->required_nb_res);
        SMPI_app_instance_register(job_id_str.c_str(), smpi_replay_process, job->required_nb_res);
    }
    XBT_INFO("SMPI applications have been registered");
}

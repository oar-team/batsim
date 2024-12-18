/*
 * @file job_submitter.cpp
 * @brief Contains functions related to job submission
 */

#include "job_submitter.hpp"

#include <vector>
#include <algorithm>
#include <boost/bind.hpp>
#include <memory>

#include <simgrid/s4u.hpp>

#include "jobs.hpp"
#include "jobs_execution.hpp"
#include "ipp.hpp"
#include "context.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(job_submitter, "job_submitter"); //!< Logging

Task* inc_child (Task *i)
{
  i->nb_parent_completed++;

  return i;
}

Task* bottom_level_f (Task *child, Task *parent)
{
  child->depth = std::max( child->depth, (parent->depth)+1 );

  return child;
}

using namespace std;

static void submit_jobs_to_server(const vector<JobPtr> & jobs_to_submit, const std::string & submitter_name)
{
    if (!jobs_to_submit.empty())
    {
        JobSubmittedMessage * msg = new JobSubmittedMessage;
        msg->submitter_name = submitter_name;
        msg->jobs = jobs_to_submit;
        send_message("server", IPMessageType::JOB_SUBMITTED, static_cast<void*>(msg));
    }
}

void static_job_submitter_process(BatsimContext * context,
                                  std::string workload_name)
{
    xbt_assert(context->workloads.exists(workload_name),
               "Error: a static_job_submitter_process is in charge of workload '%s', "
               "which does not exist", workload_name.c_str());

    Workload * workload = context->workloads.at(workload_name);

    string submitter_name = workload_name + "_submitter";

    /*  ░░░░░░░░▄▄▄███░░░░░░░░░░░░░░░░░░░░
        ░░░▄▄██████████░░░░░░░░░░░░░░░░░░░
        ░███████████████░░░░░░░░░░░░░░░░░░
        ░▀███████████████░░░░░▄▄▄░░░░░░░░░
        ░░░███████████████▄███▀▀▀░░░░░░░░░
        ░░░░███████████████▄▄░░░░░░░░░░░░░
        ░░░░▄████████▀▀▄▄▄▄▄░▀░░░░░░░░░░░░
        ▄███████▀█▄▀█▄░░█░▀▀▀░█░░▄▄░░░░░░░
        ▀▀░░░██▄█▄░░▀█░░▄███████▄█▀░░░▄░░░
        ░░░░░█░█▀▄▄▀▄▀░█▀▀▀█▀▄▄▀░░░░░░▄░▄█
        ░░░░░█░█░░▀▀▄▄█▀░█▀▀░░█░░░░░░░▀██░
        ░░░░░▀█▄░░░░░░░░░░░░░▄▀░░░░░░▄██░░
        ░░░░░░▀█▄▄░░░░░░░░▄▄█░░░░░░▄▀░░█░░
        ░░░░░░░░░▀███▀▀████▄██▄▄░░▄▀░░░░░░
        ░░░░░░░░░░░█▄▀██▀██▀▄█▄░▀▀░░░░░░░░
        ░░░░░░░░░░░██░▀█▄█░█▀░▀▄░░░░░░░░░░
        ░░░░░░░░░░█░█▄░░▀█▄▄▄░░█░░░░░░░░░░
        ░░░░░░░░░░█▀██▀▀▀▀░█▄░░░░░░░░░░░░░
        ░░░░░░░░░░░░▀░░░░░░░░░░░▀░░░░░░░░░ */

    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = false;
    hello_msg->submitter_type = SubmitterType::JOB_SUBMITTER;

    send_message("server", IPMessageType::SUBMITTER_HELLO, static_cast<void*>(hello_msg));

    long double current_submission_date = static_cast<long double>(simgrid::s4u::Engine::get_clock());

    // sort jobs by arrival date in a temporary vector
    vector<JobPtr> jobs_to_submit_vector;
    const auto & jobs = workload->jobs->jobs();
    for (const auto & mit : jobs)
    {
        const auto job = mit.second;
        jobs_to_submit_vector.push_back(job);
    }
    sort(jobs_to_submit_vector.begin(), jobs_to_submit_vector.end(), job_comparator_subtime_number);

    // put jobs to submit into a list whose elements are dropped online (for smooth refcounting-based memory clean-up)
    list<JobPtr> jobs_to_submit;
    std::copy(jobs_to_submit_vector.begin(), jobs_to_submit_vector.end(), std::back_inserter(jobs_to_submit));
    jobs_to_submit_vector.clear();

    if (jobs_to_submit.size() > 0)
    {
        vector<JobPtr> jobs_to_send;
        bool is_first_job = true;

        for ( ; !jobs_to_submit.empty() ; jobs_to_submit.pop_front())
        {
            auto job = jobs_to_submit.front();
            if (job->submission_time > current_submission_date)
            {
                // Next job submission time is after current time, send the message to the server for previous submitted jobs
                submit_jobs_to_server(jobs_to_send, submitter_name);
                jobs_to_send.clear();

                // Now let's sleep until it's time to submit the current job
                simgrid::s4u::this_actor::sleep_for(static_cast<double>(job->submission_time - current_submission_date));
                current_submission_date = static_cast<long double>(simgrid::s4u::Engine::get_clock());
            }
            // Setting the mailbox
            //job->completion_notification_mailbox = "SOME_MAILBOX";

            // Populate the vector of job identifiers to submit
            jobs_to_send.push_back(job);

            if (is_first_job)
            {
                is_first_job = false;
                if (context->energy_first_job_submission < 0)
                {
                    context->energy_first_job_submission = context->machines.total_consumed_energy(context);
                }
            }
        }

        // Send last vector of submitted jobs
        submit_jobs_to_server(jobs_to_send, submitter_name);
    }

    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->is_workflow_submitter = false;
    bye_msg->submitter_name = submitter_name;
    bye_msg->submitter_type = SubmitterType::JOB_SUBMITTER;
    send_message("server", IPMessageType::SUBMITTER_BYE, static_cast<void*>(bye_msg));
}


static string submit_workflow_task_as_job(BatsimContext *context, string workflow_name, string submitter_name, Task *task);
static string wait_for_job_completion(string submitter_name);

/* Ugly Global */
static std::map<std::string, int> task_id_counters;

void workflow_submitter_process(BatsimContext * context,
                                std::string workflow_name)
{
    // Get the workflow
    xbt_assert(context->workflows.exists(workflow_name),
               "Error: a workflow_job_submitter_process is in charge of workload '%s', "
               "which does not exist", workflow_name.c_str());
    Workflow * workflow = context->workflows.at(workflow_name);

    int limit = context->workflow_nb_concurrent_jobs_limit;
    bool not_limiting = (limit == 0);
    int current_nb = 0;

    const string submitter_name = workflow_name + "_submitter";

    XBT_INFO("New Workflow submitter for workflow %s (start time = %lf)!",
             workflow_name.c_str(),workflow->start_time);

    /* Initializing my task_id counter */
    task_id_counters[workflow->name] = 0;

    /* Hello */
    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = true;
    hello_msg->submitter_type = SubmitterType::JOB_SUBMITTER;
    send_message("server", IPMessageType::SUBMITTER_HELLO, static_cast<void*>(hello_msg));

    /* Create submitted_tasks map */
    std::map<std::string, Task *> submitted_tasks;

    /* Create ready_tasks vector */
    std::vector<Task *> ready_tasks = workflow->get_source_tasks();

    /* Wait until the workflow start-time */
    if (workflow->start_time > simgrid::s4u::Engine::get_clock())
    {
        XBT_INFO("Warning: already past workflow start time! (%lf)", workflow->start_time);
    }
    double sleep_time = workflow->start_time - simgrid::s4u::Engine::get_clock();
    simgrid::s4u::this_actor::sleep_for((0.0) > (sleep_time) ? (0.0) : (sleep_time));

    /* Submit all the ready tasks */

    while((!ready_tasks.empty())||(!submitted_tasks.empty())) /* Stops when there are no more ready tasks or tasks actually running */
    {
        while((!ready_tasks.empty())&&(not_limiting || (limit > current_nb))) /* we have some ready tasks to submit */
        {
            Task *task = ready_tasks.at(0);
            ready_tasks.erase(ready_tasks.begin() + 0);

            /* Send a Job corresponding to the Task Job */
            string job_key = submit_workflow_task_as_job(context, workflow_name, submitter_name, task);

            XBT_INFO("Inserting task %s", job_key.c_str());

            /* Insert the task into the submitted_tasks map */
            submitted_tasks[job_key] = task;
            current_nb++;
        }

        if(!submitted_tasks.empty()) /* we are done submitting tasks, wait for one to complete */
        {
            /* Wait for callback */
            string completed_job_key = wait_for_job_completion(submitter_name);
            current_nb--;

            //XBT_INFO("TASK # %s has completed!!!\n", completed_job_key.c_str());

            /* Look for the task in the map */
            Task *completed_task = submitted_tasks[completed_job_key];

            XBT_INFO("TASK %s has completed! (depth=%d)\n", completed_task->id.c_str(),completed_task->depth);

            /* All those poor hungry kids */
            std::vector<Task *> my_kids = completed_task->children;

            /* tell them they are closer to being elected */
            std::transform (my_kids.begin(),my_kids.end(),my_kids.begin(),inc_child);
            std::transform (my_kids.begin(),my_kids.end(),my_kids.begin(), boost::bind(&bottom_level_f, _1, completed_task));

            /* look for ready kids */
            for (std::vector<Task *>::iterator kiddo=my_kids.begin(); kiddo!=my_kids.end(); ++kiddo)
            {
                if((*kiddo)->nb_parent_completed==static_cast<int>((*kiddo)->parents.size()))
                {
                    ready_tasks.push_back(*kiddo);
                }
            }

            /* Nothing left for this task */
            submitted_tasks.erase(completed_job_key);
        }
    }

    double makespan = simgrid::s4u::Engine::get_clock() - workflow->start_time;
    XBT_INFO("WORKFLOW_MAKESPAN %s %lf  (depth = %d)\n", workflow->filename.c_str(), makespan, workflow->get_maximum_depth());

    /* Goodbye */
    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->is_workflow_submitter = true;
    bye_msg->submitter_type = SubmitterType::JOB_SUBMITTER;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, static_cast<void*>(bye_msg));
}

/**
 * @brief TODO
 * @param context TODO
 * @param workflow_name TODO
 * @param submitter_name TODO
 * @param task TODO
 * @return TODO
 */
static string submit_workflow_task_as_job(BatsimContext *context, string workflow_name, string submitter_name, Task *task) {

    const string workload_name = workflow_name;

    string job_number = to_string(task_id_counters[workflow_name]);
    task_id_counters[workflow_name]++;


    // Create a profile
    auto profile = make_shared<Profile>();
    profile->type = ProfileType::DELAY;
    DelayProfileData * data = new DelayProfileData;
    data->delay = task->execution_time;
    profile->data = data;
    string profile_name = workflow_name + "_" + task->id; // Create a profile name
    profile->name = profile_name;
    context->workloads.at(workload_name)->profiles->add_profile(profile_name, profile);

    // Create JSON description of Job corresponding to Task
    double walltime = task->execution_time + 10.0;
    string job_json_description = std::string() + "{" +
            "\"id\": \"" + workload_name + "!" + job_number +  "\", " +
            "\"subtime\":" + std::to_string(simgrid::s4u::Engine::get_clock()) + ", " +
            "\"walltime\":" + std::to_string(walltime) + ", " +
            "\"res\":" + std::to_string(task->num_procs) + ", " +
            "\"profile\": \"" + profile_name + "\"" +
            "}";

    // Puts the job into memory
    auto job = Job::from_json(job_json_description, context->workloads.at(workload_name),
                              "Invalid workflow-injected JSON job");
    context->workloads.at(workload_name)->jobs->add_job(job);

    // Put the metadata about the job into the data storage
    JobIdentifier job_id(workload_name, job_number);

    // Submit the job
    JobSubmittedMessage * msg = new JobSubmittedMessage;
    msg->submitter_name = submitter_name;
    msg->jobs = std::vector<JobPtr>({job});
    send_message("server", IPMessageType::JOB_SUBMITTED, static_cast<void*>(msg));

    // Return a key
    return job_id.to_string();
}

/**
 * @brief TODO
 * @param submitter_name TODO
 * @return TODO
 */
static string wait_for_job_completion(string submitter_name)
{
    IPMessage * notification = receive_message(submitter_name);

    auto * notification_data = static_cast<SubmitterJobCompletionCallbackMessage *>(notification->data);

    // TODO: memory cleanup
    return notification_data->job_id.to_string();
}


void batexec_job_launcher_process(BatsimContext * context,
                                  std::string workload_name)
{
    // TODO? or this should be completely removed?
/*    Workload * workload = context->workloads.at(workload_name);

    auto & jobs = workload->jobs->jobs();
    for (auto & mit : jobs)
    {
        auto job = mit.second;

        unsigned int nb_res = job->requested_nb_res;

        SchedulingAllocation * alloc = new SchedulingAllocation;

        alloc->job = job;
        alloc->hosts.clear();
        alloc->hosts.reserve(nb_res);
        alloc->machine_ids.clear();

        for (int i = 0; i < static_cast<int>(nb_res); ++i)
        {
            alloc->machine_ids.insert(i);
            alloc->hosts.push_back(context->machines[i]->host);
        }

        string pname = "job" + job->id.to_string();
        auto actor = simgrid::s4u::Actor::create(pname.c_str(),
                                                 context->machines[alloc->machine_ids.first_element()]->host,
                                                 execute_job_process, context, alloc, false);
        job->execution_actors.insert(actor);
    }*/
}

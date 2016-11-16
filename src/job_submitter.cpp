/*
 * @file job_submitter.cpp
 * @brief Contains functions related to job submission
 */

#include "job_submitter.hpp"

#include <vector>
#include <algorithm>
#include <boost/bind.hpp>

#include "jobs.hpp"
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

int static_job_submitter_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    JobSubmitterProcessArguments * args = (JobSubmitterProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;

    xbt_assert(context->workloads.exists(args->workload_name),
               "Error: a static_job_submitter_process is in charge of workload '%s', "
               "which does not exist", args->workload_name.c_str());

    Workload * workload = context->workloads.at(args->workload_name);

    XBT_INFO("Nom : %s", (args->workload_name).c_str() );

    string submitter_name = args->workload_name + "_submitter";

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

    send_message("server", IPMessageType::SUBMITTER_HELLO, (void*) hello_msg);

    Rational previous_submission_date = MSG_get_clock();

    vector<const Job *> jobsVector;

    const auto & jobs = workload->jobs->jobs();
    for (const auto & mit : jobs)
    {
        const Job * job = mit.second;
        jobsVector.push_back(job);
    }

    sort(jobsVector.begin(), jobsVector.end(), job_comparator_subtime);

    XBT_INFO("taille vecteur : %d", (int) jobsVector.size() );

    if (jobsVector.size() > 0)
    {
        const Job * first_submitted_job = *jobsVector.begin();

        for (const Job * job : jobsVector)
        {
            if (job->submission_time > previous_submission_date)
                MSG_process_sleep((double)(job->submission_time - previous_submission_date));
            // Setting the mailbox
            //job->completion_notification_mailbox = "SOME_MAILBOX";

            // Let's put the metadata about the job into the data storage
            JobIdentifier job_id(workload->name, job->number);
            string job_key = RedisStorage::job_key(job_id);
            string profile_key = RedisStorage::profile_key(workload->name, job->profile);
            XBT_INFO("IN STATIC JOB SUBMITTER: '%s'", job->json_description.c_str());
            context->storage.set(job_key, job->json_description);
            context->storage.set(profile_key, workload->profiles->at(job->profile)->json_description);

            // Let's now continue the simulation
            JobSubmittedMessage * msg = new JobSubmittedMessage;
            msg->submitter_name = submitter_name;
            msg->job_id.workload_name = args->workload_name;
            msg->job_id.job_number = job->number;

            send_message("server", IPMessageType::JOB_SUBMITTED, (void*)msg);
            previous_submission_date = MSG_get_clock();

            if (job == first_submitted_job)
                context->energy_first_job_submission = context->machines.total_consumed_energy(context);
        }
    }

    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->is_workflow_submitter = false;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, (void *) bye_msg);
    delete args;
    return 0;
}


static string submit_workflow_task_as_job(BatsimContext *context, string workflow_name, string submitter_name, Task *task);
static string wait_for_job_completion(string submitter_name);

/* Ugly Global */
std::map<std::string, int> task_id_counters;

int workflow_submitter_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    // Get the workflow
    WorkflowSubmitterProcessArguments * args = (WorkflowSubmitterProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;
    xbt_assert(context->workflows.exists(args->workflow_name),
               "Error: a workflow_job_submitter_process is in charge of workload '%s', "
               "which does not exist", args->workflow_name.c_str());
    Workflow * workflow = context->workflows.at(args->workflow_name);


    const string submitter_name = args->workflow_name + "_submitter";

    XBT_INFO("New Workflow submitter for workflow %s (start time = %lf)!",
                  args->workflow_name.c_str(),workflow->start_time);

    /* Initializing my task_id counter */
    task_id_counters[workflow->name] = 0;

    /* Hello */
    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = true;
    send_message("server", IPMessageType::SUBMITTER_HELLO, (void*) hello_msg);

    /* Create submitted_tasks map */
    std::map<std::string, Task *> submitted_tasks;

    /* Create ready_tasks vector */
    std::vector<Task *> ready_tasks = workflow->get_source_tasks();

    /* Wait until the workflow start-time */
    if (workflow->start_time > MSG_get_clock()) {
      XBT_INFO("Warning: already past workflow start time! (%lf)", workflow->start_time);
    }
    MSG_process_sleep(MAX(0.0, workflow->start_time - MSG_get_clock()));


    /* Submit all the ready tasks */

    while((!ready_tasks.empty())||(!submitted_tasks.empty())) /* Stops when there are no more ready tasks or tasks actually running */
    {
        while(!ready_tasks.empty()) /* we have some ready tasks to submit */
        {
            Task *task = ready_tasks.at(0);
            ready_tasks.erase(ready_tasks.begin() + 0);

            /* Send a Job corresponding to the Task Job */
            string job_key = submit_workflow_task_as_job(context, args->workflow_name, submitter_name, task);

            XBT_INFO("Inserting task %s", job_key.c_str());

            /* Insert the task into the submitted_tasks map */
            submitted_tasks[job_key] = task;
        }

        if(!submitted_tasks.empty()) /* we are done submitting tasks, wait for one to complete */
        {
            /* Wait for callback */
            string completed_job_key = wait_for_job_completion(submitter_name);

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
                if((*kiddo)->nb_parent_completed==(int)(*kiddo)->parents.size())
                    ready_tasks.push_back(*kiddo);
            }

            /* Nothing left for this task */
            submitted_tasks.erase(completed_job_key);
        }
    }

    double makespan = MSG_get_clock() - workflow->start_time;
    XBT_INFO("WORKFLOW_MAKESPAN %s %lf  (depth = %d)\n", workflow->filename.c_str(), makespan, workflow->get_maximum_depth());
    // This is a TERRIBLE exit, but the goal is to stop the simulation (don't keep simulated the workload beyond
    // the worflow completion). This is much more brutal than the -k option. To be removed/commented-out later, but right
    // now it saves a lot of time, and was obviously easy to implement. 
    exit(0);

    /* Goodbye */
    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->is_workflow_submitter = true;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, (void *) bye_msg);


    delete args;
    return 0;
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

    int job_number = task_id_counters[workflow_name];
    task_id_counters[workflow_name]++;


    // Create a profile
    Profile * profile = new Profile;
    profile->type = ProfileType::DELAY;
    DelayProfileData * data = new DelayProfileData;
    data->delay = task->execution_time;
    profile->data = data;
    profile->json_description = std::string() + "{" +
                "\"type\": \"delay\", "+
                "\"delay\": " + std::to_string(task->execution_time) +
                "}";
    string profile_name = workflow_name + "_" + task->id; // Create a profile name
    context->workloads.at(workload_name)->profiles->add_profile(profile_name, profile);

    // Create JSON description of Job corresponding to Task
    double walltime = task->execution_time + 10.0;
    string json_description = std::string() + "{" +
                            "\"id\": \"" + workload_name + "!" + std::to_string(job_number) +  "\", " +
                            "\"subtime\":" + std::to_string(MSG_get_clock()) + ", " +
                            "\"walltime\":" + std::to_string(walltime) + ", " +
                            "\"res\":" + std::to_string(task->num_procs) + ", " +
                            "\"profile\": \"" + profile_name + "\"" +
                "}";

    // Put the metadata about the job into the data storage
    JobIdentifier job_id(workload_name, job_number);
    string job_key = RedisStorage::job_key(job_id);
    string profile_key = RedisStorage::profile_key(workflow_name, profile_name);
    context->storage.set(job_key, json_description);
    context->storage.set(profile_key, profile->json_description);

    // Submit the job
    JobSubmittedMessage * msg = new JobSubmittedMessage;
    msg->submitter_name = submitter_name;
    msg->job_id.workload_name = workload_name;
    msg->job_id.job_number = job_number;
    send_message("server", IPMessageType::JOB_SUBMITTED, (void*)msg);

    // Create an ID to return
    string id_to_return = workload_name + "!" + std::to_string(job_number);


    // Return a key
    return id_to_return;

}

/**
 * @brief TODO
 * @param submitter_name TODO
 * @return TODO
 */
static string wait_for_job_completion(string submitter_name) {
    msg_task_t task_notification = NULL;
    IPMessage *task_notification_data;
    MSG_task_receive(&(task_notification), submitter_name.c_str());
    task_notification_data = (IPMessage *) MSG_task_get_data(task_notification);
    SubmitterJobCompletionCallbackMessage *notification_data =
        (SubmitterJobCompletionCallbackMessage *) task_notification_data->data;

    return  notification_data->job_id.workload_name + "!" +
            std::to_string(notification_data->job_id.job_number);

}

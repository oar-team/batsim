/*
 * @file job_submitter.cpp
 * @brief Contains functions related to job submission
 */

#include "job_submitter.hpp"

#include <vector>
#include <algorithm>

#include "jobs.hpp"
#include "ipp.hpp"
#include "context.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(job_submitter, "job_submitter"); //!< Logging


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

    double previousSubmissionDate = MSG_get_clock();

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
            if (job->submission_time > previousSubmissionDate)
                MSG_process_sleep(job->submission_time - previousSubmissionDate);
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
            previousSubmissionDate = MSG_get_clock();

            if (job == first_submitted_job)
                context->energy_first_job_submission = context->machines.total_consumed_energy(context);
        }
    }

    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, (void *) bye_msg);
    delete args;
    return 0;
}


static string submit_workflow_task_as_job(BatsimContext *context, string workflow_name, string submitter_name, Task *task);
static string wait_for_job_completion(string submitter_name);

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

    XBT_INFO("I AM A WORKFLOW SUBMITTER FOR WORKFLOW %s!", args->workflow_name.c_str());

    /* Hello */
    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = true;
    send_message("server", IPMessageType::SUBMITTER_HELLO, (void*) hello_msg);

    /* Create submitted_tasks map */
    std::map<std::string, Task *> submitted_tasks;

    /* Create ready_tasks vector */
    std::vector<Task *> ready_tasks = workflow->get_source_tasks();

    /* Pick a task */
    Task *task = ready_tasks.at(0);

    /* Send a Job corresponding to the Task Job */
    string job_key = submit_workflow_task_as_job(context, args->workflow_name, submitter_name, task);

    XBT_INFO("Inserting task %s", job_key.c_str());
    
    /* Insert the task into the submitted_tasks map */
    submitted_tasks[job_key] = task;

    /*
    std::string profile_name = args->workflow_name + "2";


    Profile * profile = new Profile;
    profile->type = ProfileType::DELAY;
    DelayProfileData * data = new DelayProfileData;
    data->delay = task->execution_time;
    profile->data = data;

    profile->json_description = std::string() + "{" +
                "\"type\": \"delay\", "+
                "\"delay\": " + std::to_string(task->execution_time) +
                "}";

    XBT_INFO("Adding a profile with name %s",profile_name.c_str());
    context->workloads.at("workflow")->profiles->add_profile(profile_name, profile);

    Job *job = new Job;
    job->workload = context->workloads.at("workflow");
    job->number = 2;
    job->profile = profile_name;
    //job->submission_time = ???
    job->walltime = task->execution_time + 10.0; // hack
    job->required_nb_res = task->num_procs;
    job->json_description = std::string() + "{" +
                            "\"id\": 2" +  ", " +
                            "\"subtime\":" + std::to_string(MSG_get_clock()) + ", " +
                            "\"walltime\":" + std::to_string(job->walltime) + ", " +
                            "\"res\":" + std::to_string(job->required_nb_res) + ", " +
                            "\"profile\": \"" + profile_name + "\"" +
                "}";

    //context->workloads.at("workflow")->jobs->add_job(job);

    //job->consumed_energy = ???
    //job->starting_time = ???
    job->runtime = task->execution_time;

    // Let's put the metadata about the job into the data storage
    JobIdentifier job_id(workflow->name, 2);
    string job_key = RedisStorage::job_key(job_id);
    string profile_key = RedisStorage::profile_key(workflow->name, job->profile);
    context->storage.set(job_key, job->json_description);
    context->storage.set(profile_key, profile->json_description);

    // Let's now continue the simulation
    JobSubmittedMessage * msg = new JobSubmittedMessage;
    msg->submitter_name = submitter_name;
    msg->job_id.workload_name = "workflow";
    msg->job_id.job_number = 2;

    send_message("server", IPMessageType::JOB_SUBMITTED, (void*)msg);

    */

    /* Wait for callback */
    string completed_job_key = wait_for_job_completion(submitter_name);

    XBT_INFO("TASK # %s has completed!!!\n", completed_job_key.c_str());

    /* Look for the task in the map */
    Task *completed_task = submitted_tasks[completed_job_key];

    XBT_INFO("TASK %s has completed!!!\n", completed_task->id.c_str());


/*
    /// WAIT FOR CALLBACK
    msg_task_t task_notification = NULL;
    IPMessage *task_notification_data;
    MSG_task_receive(&(task_notification), submitter_name.c_str());
    task_notification_data = (IPMessage *) MSG_task_get_data(task_notification);
    SubmitterJobCompletionCallbackMessage *notification_data =
        (SubmitterJobCompletionCallbackMessage *) task_notification_data->data;

    XBT_INFO("GOT A COMPLETION NOTIFICATION FOR JOB ID: %s %d",
                notification_data->job_id.workload_name.c_str(),
                notification_data->job_id.job_number);
*/


    /* Goodbye */
    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
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

    static int job_number = 0;    // "glogal" to ensure unique job numbers in job_ids
    const string workload_name = workflow_name;

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

    // Increment the job_number
    job_number++;

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

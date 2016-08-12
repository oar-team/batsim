/**
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
    const string submitter_name = "static_submitter";

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


int workflow_submitter_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    WorkflowSubmitterProcessArguments * args = (WorkflowSubmitterProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;
    (void) context; // Avoids a unused warning, for future Travis builds

    /* Not needed here for now, since the workflow filename was
     * passed as an argument, and has already been checked

    xbt_assert(context->workloads.exists(args->workload_name),
               "Error: a static_job_submitter_process is in charge of workload '%s', "
               "which does not exist", args->workload_name.c_str());
     */

    /*
    const char *workflow_name = args->workflow_name;
    (void) workflow_name; // Silences unused warning
    */

    // Get the workflow
    xbt_assert(context->workflows.exists(args->workflow_name),
               "Error: a workflow_job_submitter_process is in charge of workload '%s', "
               "which does not exist", workflow_name.c_str());

    Workflow * workflow = context->workflows.at(args->workflow_name);

    const string submitter_name = "workflow_submitter";
 

    XBT_INFO("I AM A WORKFLOW SUBMITTER FOR WORKFLOW %s!", workflow_name);
    XBT_INFO("This workflow has %d tasks!", workflow.tasks.size());

    SubmitterHelloMessage * hello_msg = new SubmitterHelloMessage;
    hello_msg->submitter_name = submitter_name;
    hello_msg->enable_callback_on_job_completion = true;

    send_message("server", IPMessageType::SUBMITTER_HELLO, (void*) hello_msg);

    /* Send a Bogus Job and wait for the notification */
    //Job bogus_job = new Job;

   /*

    double previousSubmissionDate = MSG_get_clock();

    vector<const Job *> jobsVector;

    const auto & jobs = workload->jobs->jobs();
    for (const auto & mit : jobs)
    {
        const Job * job = mit.second;
        jobsVector.push_back(job);
    }

    sort(jobsVector.begin(), jobsVector.end(), job_comparator_subtime);

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
            string job_id_string = args->workload_name + "!" + to_string(job->number);
            string job_key = "job_" + job_id_string;
            string profile_key = "profile_" + job_id_string;
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

    */

    SubmitterByeMessage * bye_msg = new SubmitterByeMessage;
    bye_msg->submitter_name = submitter_name;
    send_message("server", IPMessageType::SUBMITTER_BYE, (void *) bye_msg);


    delete args;
    return 0;
}

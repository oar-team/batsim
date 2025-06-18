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
    bye_msg->submitter_name = submitter_name;
    bye_msg->submitter_type = SubmitterType::JOB_SUBMITTER;
    send_message("server", IPMessageType::SUBMITTER_BYE, static_cast<void*>(bye_msg));
}

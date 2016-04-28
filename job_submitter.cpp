/**
 * @file job_submitter.hpp
 * @brief Job submitter
 */

#include "job_submitter.hpp"

#include <vector>
#include <algorithm>

#include "jobs.hpp"
#include "ipp.hpp"
#include "context.hpp"

using namespace std;

int job_submitter_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    JobSubmitterProcessArguments * args = (JobSubmitterProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;

    send_message("server", IPMessageType::SUBMITTER_HELLO);

    double previousSubmissionDate = MSG_get_clock();

    vector<const Job *> jobsVector;

    const auto & jobs = context->jobs.jobs();
    for (const auto & mit : jobs)
    {
        const Job * job = mit.second;
        jobsVector.push_back(job);
    }

    sort(jobsVector.begin(), jobsVector.end(), job_comparator_subtime);

    for (const Job * job : jobsVector)
    {
        if (job->submission_time > previousSubmissionDate)
            MSG_process_sleep(job->submission_time - previousSubmissionDate);

        JobSubmittedMessage * msg = new JobSubmittedMessage;
        msg->job_id = job->id;

        send_message("server", IPMessageType::JOB_SUBMITTED, (void*)msg);
        previousSubmissionDate = MSG_get_clock();
    }

    send_message("server", IPMessageType::SUBMITTER_BYE);
    delete args;
    return 0;
}

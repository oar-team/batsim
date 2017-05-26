/**
 * @file server.hpp
 * @brief Contains functions related to the general orchestration of the simulation
 */

#pragma once

#include <string>
#include <map>

#include "ipp.hpp"

struct BatsimContext;

/**
 * @brief Data associated with the server_process process
 */
struct ServerData
{
    /**
     * @brief Data associated with the job submitters (used for callbacks)
     */
    struct Submitter
    {
        std::string mailbox;        //!< The Submitter mailbox
        bool should_be_called_back; //!< Whether the submitter should be notified on events.
    };

    BatsimContext * context = nullptr; //!< The BatsimContext

    int nb_completed_jobs = 0;  //!< The number of completed jobs
    int nb_submitted_jobs = 0;  //!< The number of submitted jobs
    int nb_submitters = 0;  //!< The number of submitters
    int nb_submitters_finished = 0; //!< The number of finished submitters
    int nb_workflow_submitters_finished = 0;    //!< The number of finished workflow submitters
    int nb_running_jobs = 0;    //!< The number of jobs being executed
    int nb_switching_machines = 0;  //!< The number of machines being switched
    int nb_waiters = 0; //!< The number of pending CALL_ME_LATER waiters
    int nb_killers = 0; //!< The number of killers
    bool sched_ready = true;    //!< Whether the scheduler can be called now
    bool all_jobs_submitted_and_completed = false;  //!< Whether all jobs (static and dynamic) have been submitted and completed.
    bool end_of_simulation_sent = false; //!< Whether the SIMULATION_ENDS event has been sent to the scheduler.

    std::map<std::string, Submitter*> submitters;   //!< The submitters
    std::map<JobIdentifier, Submitter*> origin_of_jobs; //!< Stores whether a Submitter must be notified on job completion
    //map<std::pair<int,double>, Submitter*> origin_of_wait_queries;
};

/**
 * @brief Process used to orchestrate the simulation
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int server_process(int argc, char *argv[]);

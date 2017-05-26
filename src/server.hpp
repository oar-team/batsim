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
 * @brief Checks whether all jobs are submitted and completed
 * @param[in,out] data The data associated with the server_process
 */
void check_submitted_and_completed(ServerData * data);

/**
 * @brief Process used to orchestrate the simulation
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int server_process(int argc, char *argv[]);


/**
 * @brief Server SUBMITTER_HELLO handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_submitter_hello(ServerData * data,
                               IPMessage * task_data);

/**
 * @brief Server SUBMITTER_BYE handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_submitter_bye(ServerData * data,
                             IPMessage * task_data);

/**
 * @brief Server JOB_COMPLETED handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_job_completed(ServerData * data,
                             IPMessage * task_data);

/**
 * @brief Server JOB_SUBMITTED handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_job_submitted(ServerData * data,
                             IPMessage * task_data);

/**
 * @brief Server PSTATE_MODIFICATION handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_pstate_modification(ServerData * data,
                                   IPMessage * task_data);

/**
 * @brief Server WAITING_DONE handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_waiting_done(ServerData * data,
                            IPMessage * task_data);

/**
 * @brief Server SCHED_READY handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_sched_ready(ServerData * data,
                           IPMessage * task_data);

/**
 * @brief Server SCHED_WAIT_ANSWER handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_sched_wait_answer(ServerData * data,
                                 IPMessage * task_data);

/**
 * @brief Server SCHED_TELL_ME_ENERGY handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_sched_tell_me_energy(ServerData * data,
                                    IPMessage * task_data);

/**
 * @brief Server WAIT_QUERY handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_wait_query(ServerData * data,
                          IPMessage * task_data);

/**
 * @brief Server SWITCHED_ON handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_switched_on(ServerData * data,
                           IPMessage * task_data);

/**
 * @brief Server SWITCHED_OFF handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_switched_off(ServerData * data,
                            IPMessage * task_data);

/**
 * @brief Server KILLING_DONE handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_killing_done(ServerData * data,
                            IPMessage * task_data);

/**
 * @brief Server END_DYNAMIC_SUBMIT handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_end_dynamic_submit(ServerData * data,
                                  IPMessage * task_data);


/**
 * @brief Server JOB_SUBMITTED_BY_DP handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_submit_job(ServerData * data,
                          IPMessage * task_data);

/**
 * @brief Server SCHED_REJECT_JOB handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_reject_job(ServerData * data,
                          IPMessage * task_data);

/**
 * @brief Server SCHED_KILL_JOB handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_kill_jobs(ServerData * data,
                         IPMessage * task_data);

/**
 * @brief Server SCHED_CALL_ME_LATER handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_call_me_later(ServerData * data,
                             IPMessage * task_data);

/**
 * @brief Server SCHED_EXECUTE_JOB handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_execute_job(ServerData * data,
                           IPMessage * task_data);

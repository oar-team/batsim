/**
 * @file server.hpp
 * @brief Contains functions related to the general orchestration of the simulation
 */

#pragma once

#include <string>
#include <map>

#include <simgrid/s4u.hpp>

#include "ipp.hpp"

struct BatsimContext;

/**
 * @brief Data associated with the server_process process
 */
struct ServerData
{
    /**
     * @brief Data associated with the job and event submitters (used for callbacks)
     */
    struct Submitter
    {
        std::string mailbox;            //!< The Submitter mailbox
        bool should_be_called_back;     //!< Whether the submitter should be notified on events.
        SubmitterType submitter_type;   //!< The type of the submitter
    };

    /**
     * @brief Various counters associated with submitters
     */
    struct SubmitterCounters
    {
        unsigned int expected_nb_submitters = static_cast<unsigned int>(-1); //!< The expected number of submitters
        unsigned int nb_submitters = 0; //!< The number of submitters
        unsigned int nb_submitters_finished = 0; //!< The number of finished submitters
    };

    BatsimContext * context = nullptr; //!< The BatsimContext

    int nb_completed_jobs = 0;  //!< The number of completed jobs
    int nb_submitted_jobs = 0;  //!< The number of submitted jobs
    int nb_running_jobs = 0;    //!< The number of jobs being executed
    int nb_workflow_submitters_finished = 0; //!< The number of finished workflow submitters
    int nb_switching_machines = 0;  //!< The number of machines being switched
    int nb_callmelater_entities = 0; //!< The number of alive entities to handle CALL_ME_LATER (dedicated actors for OneShot, a part of the periodic actor for Periodic)
    int nb_probe_entities = 0; //!< The number of alive entities to handle probes
    bool sched_ready = true;    //!< Whether the scheduler can be called now
    bool sched_said_hello = false; //!< Whether the scheduler said hello

    bool simulation_stop_asked = false;  //!< Whether a FORCE_SIMULATION_STOP event has been received
    bool end_of_simulation_sent = false; //!< Whether the SIMULATION_ENDS event has been sent to the scheduler
    bool end_of_simulation_ack_received = false; //!< Whether the SIMULATION_ENDS acknowledgement (empty message) has been received

    std::map<std::string, Submitter*> submitters;   //!< The submitters
    std::unordered_map<SubmitterType, SubmitterCounters> submitter_counters; //!< A map of counters for Job, Event and Workflow Submitters
    std::map<JobIdentifier, Submitter*> origin_of_jobs; //!< Stores whether a Submitter must be notified on job completion
    std::vector<JobIdentifier> jobs_to_be_deleted; //!< Stores the job_ids to be deleted after sending a message
    std::unordered_map<KillJobsMessage *, simgrid::s4u::ActorPtr> killer_actors; //!< Stores the SimGrid killer_process actors
    simgrid::s4u::ActorPtr sched_req_rep_actor; //!< Stores the SimGrid sched-req-rep actor (edc_decisions_injector)

};

/**
 * @brief Returns whether the simulation is finished or not.
 * @param[in] data The ServerData
 * @return Whether the simulation is finished or not
 */
bool is_simulation_finished(ServerData * data);

/**
 * @brief Finish current message, call take_decisions on the external decision component and spawn another actor to inject the decisions into the simulation
 * @param[in,out] data The data associated with the server_process
 */
void finish_message_and_call_edc(ServerData * data);

/**
 * @brief Process used to orchestrate the simulation
 * @param[in] context The BatsimContext
 */
void server_process(BatsimContext * context);


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
 * @brief Internal Server handler for machine_unavailable external event
 * @param[in,out] data The data associated with the server_process
 * @param[in] event The Event involved
 */
void server_on_event_machine_unavailable(ServerData * data,
                                     const Event * event);

/**
 * @brief Internal Server handler for machine_available external event
 * @param[in,out] data The data associated with the server_process
 * @param[in] event The Event involved
 */
void server_on_event_machine_available(ServerData * data,
                                     const Event * event);


/**
 * @brief Server handler for Event of type EVENT_GENERIC
 * @param[in,out] data The data associated with the server_process
 * @param event The event that occurred
 */
void server_on_event_generic(ServerData * data,
                             const Event * event);

/**
 * @brief Server EVENT_OCCURRED handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_event_occurred(ServerData * data,
                              IPMessage * task_data);


/**
 * @brief Server PSTATE_MODIFICATION handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_pstate_modification(ServerData * data,
                                   IPMessage * task_data);

/**
 * @brief Server ONESHOT_REQUESTED_CALL handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_oneshot_requested_call(ServerData * data,
                                      IPMessage * task_data);

/**
 * @brief Server PERIODIC_TRIGGER handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_periodic_trigger(ServerData * data,
                                IPMessage * task_data);

/**
 * @brief Server PERIODIC_ENTITY_STOPPED handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_periodic_entity_stopped(ServerData * data,
                                       IPMessage * task_data);

/**
 * @brief Server SCHED_READY handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_sched_ready(ServerData * data,
                           IPMessage * task_data);


/**
 * @brief Server SWITCHED_ON/SWITCHED_OFF handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_switched(ServerData * data,
                           IPMessage * task_data);

/**
 * @brief Server KILLING_DONE handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_killing_done(ServerData * data,
                            IPMessage * task_data);

/**
 * @brief Server SCHED_END_DYNAMIC_REGISTRATION handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_end_dynamic_registration(ServerData * data,
                                        IPMessage * task_data);

/**
 * @brief Server SCHED_FORCE_SIMULATION_STOP handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_force_simulation_stop(ServerData * data,
                                     IPMessage * task_data);

/**
 * @brief Server SCHED_JOB_REGISTERED handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_register_job(ServerData * data,
                          IPMessage * task_data);

/**
 * @brief Server SCHED_PROFILE_REGISTERED handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_register_profile(ServerData * data,
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
 * @brief Server CREATE_PROBE handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_create_probe(ServerData * data,
                           IPMessage * task_data);

/**
 * @brief Server STOP_PROBE handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_stop_probe(ServerData * data,
                          IPMessage * task_data);

/**
 * @brief Server SCHED_CALL_ME_LATER handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_call_me_later(ServerData * data,
                             IPMessage * task_data);

/**
 * @brief Server SCHED_STOP_CALL_ME_LATER handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_stop_call_me_later(ServerData * data,
                                  IPMessage * task_data);

/**
 * @brief Server SCHED_EXECUTE_JOB handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_execute_job(ServerData * data,
                           IPMessage * task_data);

/**
 * @brief Server SCHED_HELLO handler
 * @param[in,out] data The data associated with the server_process
 * @param[in,out] task_data The data associated with the message the server received
 */
void server_on_edc_hello(ServerData * data, IPMessage * task_data);

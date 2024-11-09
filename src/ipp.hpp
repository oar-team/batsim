/**
 * @file ipp.hpp
 * @brief Inter-Process Protocol (within Batsim, not with the Decision real process)
 */

#pragma once

#include <vector>
#include <map>
#include <string>

#include <rapidjson/document.h>

#include <simgrid/s4u.hpp>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include "pointers.hpp"
#include "jobs.hpp"
#include "events.hpp"

struct BatsimContext;
struct ServerData;

/**
 * @brief Stores the different types of inter-process messages
 */
enum class IPMessageType
{
    JOB_SUBMITTED          //!< Submitter -> Server. The submitter tells the server that one or several new jobs have been submitted.
    ,JOB_REGISTERED_BY_DP     //!< Scheduler -> Server. The scheduler tells the server that the decision process wants to register a job
    ,PROFILE_REGISTERED_BY_DP //!< Scheduler -> Server. The scheduler tells the server that the decision process wants to register a profile
    ,JOB_COMPLETED          //!< Launcher -> Server. The job launcher tells the server a job has been completed.
    ,PSTATE_MODIFICATION    //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (modify the state of some resources).
    ,SCHED_EXECUTE_JOB      //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (execute a job).
    ,SCHED_CHANGE_JOB_STATE //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (change the state of a job).
    ,SCHED_REJECT_JOB       //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (reject a job).
    ,SCHED_KILL_JOBS         //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (kill a job).
    ,SCHED_HELLO            //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (say hello).
    ,SCHED_CALL_ME_LATER    //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (the scheduler wants to be called in the future).
    ,SCHED_STOP_CALL_ME_LATER //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (the scheduler no longer wants to be called in the future).
    ,SCHED_TELL_ME_ENERGY   //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (the scheduler wants to know the platform consumed energy).
    ,SCHED_WAIT_ANSWER      //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (a WAIT_ANSWER message).
    ,WAIT_QUERY             //!< Server -> Scheduler. The scheduler tells the server a scheduling event occured (a WAIT_ANSWER message).
    ,SCHED_READY            //!< Scheduler -> Server. The scheduler tells the server that the scheduler is ready (the scheduler is ready, messages can be sent to it).
    ,ONESHOT_REQUESTED_CALL //!< OneShot -> Server. The target time of a OneShot requested call has been reached.
    ,PERIODIC_TRIGGER       //!< Periodic -> Server. The target time of periodic events has been reached, which has has triggered events.
    ,KILLING_DONE           //!< Killer -> Server. The killer tells the server that all the jobs have been killed.
    ,SUBMITTER_HELLO        //!< Submitter -> Server. The submitter tells it starts submitting to the server.
    ,SUBMITTER_CALLBACK     //!< Server -> Submitter. The server sends a message to the Submitter. This message is initiated when a Job which has been submitted by the submitter has completed. The submitter must have said that it wanted to be called back when he said hello.
    ,SUBMITTER_BYE          //!< Submitter -> Server. The submitter tells it stops submitting to the server.
    ,SWITCHED_ON            //!< SwitcherON -> Server. The switcherON process tells the server the machine pstate has been changed
    ,SWITCHED_OFF           //!< SwitcherOFF -> Server. The switcherOFF process tells the server the machine pstate has been changed.
    ,END_DYNAMIC_REGISTER     //!< Scheduler -> Server. The scheduler tells the server that dynamic job submissions are finished.
    ,EVENT_OCCURRED            //!< Sumbitter -> Server. The event submitter tells the server that one or several events have occurred.
    ,DIE                        //!< Server -> Periodic. The server asks the periodic trigger manager to stop.
};

/**
 * @brief Contains the different types of submitters
 */
enum class SubmitterType
{
     JOB_SUBMITTER              //!< A Job submitter
    ,EVENT_SUBMITTER            //!< An Event submitter
};

/// @cond DOXYGEN_SHOULD_SKIP_THIS
// Required by old C++ to use SubmitterType as a key type in a hashmap
namespace std
{
    template <> struct hash<SubmitterType>
    {
        size_t operator() (const SubmitterType &t) const { return size_t(t); }
    };
}
/// @endcond

/**
 * @brief The content of the SUBMITTER_HELLO message
 */
struct SubmitterHelloMessage
{
    std::string submitter_name;             //!< The name of the submitter. Must be unique. Is also used as a mailbox.
    bool enable_callback_on_job_completion; //!< If set to true, the submitter should be called back when its jobs complete.
    SubmitterType submitter_type;           //!< The type of the Submitter
};

/**
 * @brief The content of the SUBMITTER_BYE message
 */
struct SubmitterByeMessage
{
    std::string submitter_name;     //!< The name of the submitter.
    SubmitterType submitter_type;   //!< The type of the submitter.
    bool is_workflow_submitter;     //!< Stores whether the finished submitter was a Workflow submitter
};

/**
 * @brief The content of the SUBMITTER_CALLBACK message
 */
struct SubmitterJobCompletionCallbackMessage
{
    JobIdentifier job_id; //!< The JobIdentifier
};

/**
 * @brief The content of the JobSubmitted message
 */
struct JobSubmittedMessage
{
    std::string submitter_name; //!< The name of the submitter which submitted the jobs.
    std::vector<JobPtr> jobs; //!< The list of submitted Jobs
};

/**
 * @brief The content of the JobRegisteredByDP message
 */
struct JobRegisteredByDPMessage
{
    JobPtr job; //!< The freshly registered job
    std::string job_description; //!< The job description string
};

/**
 * @brief The content of the ProfileRegisteredByDPMessage message
 */
struct ProfileRegisteredByDPMessage
{
    std::string workload_name; //!< The workload name
    std::string profile_name; //!< The profile name
    std::string profile; //!< The registered profile data
};

/**
 * @brief The content of the SetJobMetadataMessage message
 */
struct SetJobMetadataMessage
{
    JobIdentifier job_id; //!< The JobIdentifier of the job whose metedata should be changed
    std::string metadata; //!< The job metadata string
};

/**
 * @brief The content of the JobCompleted message
 */
struct JobCompletedMessage
{
    JobPtr job; //!< The Job that has completed
};

/**
 * @brief The content of the ChangeJobState message
 */
struct ChangeJobStateMessage
{
    JobIdentifier job_id; //!< The JobIdentifier
    std::string job_state; //!< The new job state
};

/**
 * @brief The content of the JobRejected message
 */
struct RejectJobMessage
{
    JobPtr job; //!< The Job to reject
};

/**
 * @brief A host allocation with an executor placement on it
 */
struct AllocationPlacement
{
    IntervalSet hosts; //!< The allocated SimGrid hosts
    bool use_predefined_strategy; //!< Whether to use a predefined strategy or a custom mapping for placement
    batprotocol::fb::PredefinedExecutorPlacementStrategy predefined_strategy; //!< The predefined placement strategy
    std::vector<unsigned int> custom_mapping; //!< The custom placement mapping. The vector size must match the amount of SimGrid executors, which is defined by the (profile, host_allocation) tuple. The value at index i is used to determine where SimGrid executor i runs. Values are NOT host resource ids, they are unsigned integers in [0, size(host_allocation)[ that are used to refer to hosts indirectly via host_allocation.
};

/**
 * @brief The content of the EXECUTE_JOB message
 */
struct ExecuteJobMessage
{
    JobPtr job; //!< The Job to execute
    std::shared_ptr<AllocationPlacement> job_allocation; //!< The main allocation/placement for the job.
    std::map<std::string, std::shared_ptr<AllocationPlacement> > profile_allocation_override; //!< Optional overrides for the allocation/placement of each profile within the job.
    std::map<std::string, int> storage_mapping; //!< Mapping from label given in the profile and machine id
};

/**
 * @brief The content of the KILL_JOB message
 */
struct KillJobsMessage
{
    std::vector<JobPtr> jobs; //!< The jobs to kill. Some jobs can be removed from this vector to avoid double kills.
    std::vector<std::string> job_ids; //!< IDs of the jobs to kill. This is kept separated from jobs as job_ids will be used to ACK the kills even if all jobs have not been killed (some may have finished in the meantime).
};

/**
 * @brief Wrapper struct around batprotocol::fb::EDCRequestedSimulationFeatures
 */
struct EDCRequestedSimulationFeatures
{
    bool dynamic_registration = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
    bool profile_reuse = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
    bool acknowledge_dynamic_jobs = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
    bool forward_profiles_on_job_submission = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
    bool forward_profiles_on_jobs_killed = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
    bool forward_profiles_on_simulation_begins = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
    bool forward_unknown_external_events = false; //!< cf. batprotocol::fb::EDCRequestedSimulationFeatures
};

/**
 * @brief The content of the EDC_HELLO message
 */
struct EDCHelloMessage
{
    std::string batprotocol_version; //!< The batprotocol version used by the external decision component
    std::string edc_name; //!< The name of the external decision component
    std::string edc_version; //!< The version of the external decision component
    std::string edc_commit; //!< The commit of the external decision component
    EDCRequestedSimulationFeatures requested_simulation_features; //!< The simulation features requested by this EDC
};

/**
 * @brief The content of the PstateModification message
 */
struct PStateModificationMessage
{
    IntervalSet machine_ids; //!< The IDs of the machines on which the pstate should be changed
    int new_pstate = -1; //!< The power state into which the machines should be put
};

/**
 * @brief Wrapper struct around batprotocol::fb::Periodic
 */
struct Periodic
{
    uint64_t period;
    uint64_t offset = 0;
    batprotocol::fb::TimeUnit time_unit = batprotocol::fb::TimeUnit_Second;
    bool is_infinite;
    unsigned int nb_periods;
};

/**
 * @brief The content of the CallMeLater message
 */
struct CallMeLaterMessage
{
    std::string call_id; //!< The identifier that will be used to send the calls
    bool is_periodic; //!< Whether the future call is periodic or not
    double target_time = -1; //!< For a non-periodic call, this is the time at which Batsim should call the EDC
    batprotocol::fb::TimeUnit time_unit = batprotocol::fb::TimeUnit_Second; //!< For a non-periodic call, this is the time unit used by target_time
    Periodic periodic; //!< Defines all periodic information for periodic calls
};

struct RequestedCall
{
    std::string call_id; //!< The identifier that will be used to send the calls
    bool is_last_periodic_call = false; //!< Whether this message comes from the last call of a non-infinite periodic call
};

struct OneShotRequestedCallMessage
{
    RequestedCall call;
};

struct StopCallMeLaterMessage
{
    std::string call_id; //!< The identifier of the CALL_ME_LATER to stop
};

struct PeriodicTriggerMessage
{
    std::vector<RequestedCall> calls;
    // TODO: probes
};

/**
 * @brief The content of the WaitQuery message
 */
struct WaitQueryMessage
{
    std::string submitter_name; //!< The name of the submitter which submitted the job.
    int nb_resources;    //!< The number of resources for which we would like to know the waiting time
    double processing_time; //!< The duration for which the resources would be used
};

/**
 * @brief The content of the SchedWaitAnswer message
 */
struct SchedWaitAnswerMessage
{
    std::string submitter_name; //!< The name of the submitter which submitted the job.
    int nb_resources;    //!< The number of resources for which we would like to know the waiting time
    double processing_time; //!< The duration for which the resources would be used
    double expected_time; //!< The expected waiting time supplied by the scheduler
};

/**
 * @brief The content of the SwitchON/SwitchOFF message
 */
struct SwitchMessage
{
    int machine_id = -1; //!< The unique number of the machine which should be switched ON
    int new_pstate = -1; //!< The power state the machine should be put into
};

/**
 * @brief The content of the KillingDone message
 */
struct KillingDoneMessage
{
    KillJobsMessage * kill_jobs_message = nullptr; //!< The KillJobsMessage that initiated the kills
    std::map<std::string, std::shared_ptr<batprotocol::KillProgress>> jobs_progress; //!< Stores the progress of the jobs that have really been killed
    bool acknowledge_kill_on_protocol = false; //!< Whether to send a JOB_KILLED event to acknowledge the kills

    ~KillingDoneMessage();
};

/**
 * @brief The content of the ToJobMessage message
 */
struct ToJobMessage
{
    JobIdentifier job_id; //!< The JobIdentifier
    std::string message; //!< The message to send to the job
};

/**
 * @brief The content of the FromJobMessage message
 */
struct FromJobMessage
{
    JobIdentifier job_id; //!< The JobIdentifier
    rapidjson::Document message; //!< The message to send to the scheduler
};

/**
 * @brief The content of the EventOccurred message
 */
struct EventOccurredMessage
{
    std::string submitter_name;          //!< The name of the submitter which submitted the events.
    std::vector<const Event *> occurred_events; //!< The list of Event that occurred
};

/**
 * @brief The base struct sent in inter-process messages
 */
struct IPMessage
{
    /**
     * @brief Destroys a IPMessage
     * @details This method deletes the message data according to its type
     */
    ~IPMessage();
    IPMessageType type; //!< The message type
    void * data; //!< The message data. Can be NULL for message types without associated data. Otherwise, the C++ type depends on the message type.
};

/**
 * @brief Just an IPMessage with its associated timestamp
 */
struct IPMessageWithTimestamp
{
    IPMessage * message = nullptr; //!< The IPMessage
    double timestamp = -1; //!< The timestamp
};

void generic_send_message(
    const std::string & destination_mailbox,
    IPMessageType type,
    void * data,
    bool detached
);

void send_message_at_time(
    const std::string & destination_mailbox,
    IPMessage * message,
    double when,
    bool detached = false
);

void send_message_at_time(
    const std::string & destination_mailbox,
    IPMessageType type,
    void * data,
    double when,
    bool detached = false
);

void send_message(const std::string & destination_mailbox, IPMessageType type, void * data = nullptr);
void send_message(const char * destination_mailbox, IPMessageType type, void * data = nullptr);

void dsend_message(const std::string & destination_mailbox, IPMessageType type, void * data = nullptr);
void dsend_message(const char * destination_mailbox, IPMessageType type, void * data = nullptr);

IPMessage * receive_message(const std::string & reception_mailbox);

bool mailbox_empty(const std::string & reception_mailbox);

std::string ip_message_type_to_string(IPMessageType type);
std::string submitter_type_to_string(SubmitterType type);

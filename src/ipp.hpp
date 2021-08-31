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
    ,SCHED_KILL_JOB         //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (kill a job).
    ,SCHED_CALL_ME_LATER    //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (the scheduler wants to be called in the future).
    ,SCHED_TELL_ME_ENERGY   //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (the scheduler wants to know the platform consumed energy).
    ,SCHED_WAIT_ANSWER      //!< Scheduler -> Server. The scheduler tells the server a scheduling event occured (a WAIT_ANSWER message).
    ,WAIT_QUERY             //!< Server -> Scheduler. The scheduler tells the server a scheduling event occured (a WAIT_ANSWER message).
    ,SCHED_READY            //!< Scheduler -> Server. The scheduler tells the server that the scheduler is ready (the scheduler is ready, messages can be sent to it).
    ,WAITING_DONE           //!< Waiter -> Server. The waiter tells the server that the target time has been reached.
    ,KILLING_DONE           //!< Killer -> Server. The killer tells the server that all the jobs have been killed.
    ,SUBMITTER_HELLO        //!< Submitter -> Server. The submitter tells it starts submitting to the server.
    ,SUBMITTER_CALLBACK     //!< Server -> Submitter. The server sends a message to the Submitter. This message is initiated when a Job which has been submitted by the submitter has completed. The submitter must have said that it wanted to be called back when he said hello.
    ,SUBMITTER_BYE          //!< Submitter -> Server. The submitter tells it stops submitting to the server.
    ,SWITCHED_ON            //!< SwitcherON -> Server. The switcherON process tells the server the machine pstate has been changed
    ,SWITCHED_OFF           //!< SwitcherOFF -> Server. The switcherOFF process tells the server the machine pstate has been changed.
    ,END_DYNAMIC_REGISTER     //!< Scheduler -> Server. The scheduler tells the server that dynamic job submissions are finished.
    ,EVENT_OCCURRED            //!< Sumbitter -> Server. The event submitter tells the server that one or several events have occurred.
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
struct JobRejectedMessage
{
    JobIdentifier job_id; //!< The JobIdentifier
};

/**
 * @brief A subpart of the SchedulingAllocation message
 */
struct SchedulingAllocation
{
    JobPtr job; //!< The Job to execute
    IntervalSet machine_ids; //!< User defined allocation in range of machines ids
    std::vector<int> mapping; //!< The mapping from executors (~=ranks) to resource ids. Can be empty, in which case it will NOT be used (a round robin will be used instead). If not empty, must be of the same size of the job, and each value must be in [0,nb_allocated_res[.
    std::map<std::string, int> storage_mapping; //!< mapping from label given in the profile and machine id
    std::vector<simgrid::s4u::Host*> hosts;  //!< The list of SimGrid hosts that would be used (one executor per host)
    std::vector<simgrid::s4u::Host*> io_hosts;  //!< The list of SimGrid hosts that would be used for additional io job that will be merged to the job (one executor per host)
    IntervalSet io_allocation; //!< The user defined additional io allocation in machine ids
};

/**
 * @brief The content of the EXECUTE_JOB message
 */
struct ExecuteJobMessage
{
    SchedulingAllocation * allocation; //!< The allocation itself
    ProfilePtr io_profile = nullptr; //!< The optional io profile
};

/**
 * @brief The content of the KILL_JOB message
 */
struct KillJobMessage
{
    std::vector<JobIdentifier> jobs_ids; //!< The ids of the jobs to kill
};

/**
 * @brief The content of the PstateModification message
 */
struct PStateModificationMessage
{
    IntervalSet machine_ids; //!< The IDs of the machines on which the pstate should be changed
    int new_pstate; //!< The power state into which the machines should be put
};

/**
 * @brief The content of the CallMeLater message
 */
struct CallMeLaterMessage
{
    double target_time; //!< The time at which Batsim should send a message to the decision real process
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
    int machine_id; //!< The unique number of the machine which should be switched ON
    int new_pstate; //!< The power state the machine should be put into
};

/**
 * @brief The content of the KillingDone message
 */
struct KillingDoneMessage
{
    std::vector<JobIdentifier> jobs_ids; //!< The IDs of the jobs whose kill has been requested
    std::map<std::string, std::shared_ptr<batprotocol::KillProgress>> jobs_progress; //!< Stores the progress of the jobs that have really been killed.
    bool acknowledge_kill_on_protocol; //!< Whether to send a JOB_KILLED event to acknowledge the kills
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
    void * data;        //!< The message data (can be NULL if type is in [SCHED_NOP, SUBMITTER_HELLO, SUBMITTER_BYE, SUBMITTER_READY]). Otherwise, it is either a JobSubmittedMessage*, a JobCompletedMessage* or a SchedulingAllocationMessage* according to type.
};

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of the message to send
 * @param[in] data The data associated with the message
 * @param[in] detached Whether the send should be detached (put or put_async)
 */
void generic_send_message(const std::string & destination_mailbox,
                          IPMessageType type,
                          void * data,
                          bool detached);
/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void send_message(const std::string & destination_mailbox, IPMessageType type, void * data = nullptr);

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void send_message(const char * destination_mailbox, IPMessageType type, void * data = nullptr);

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void dsend_message(const std::string & destination_mailbox, IPMessageType type, void * data = nullptr);

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] destination_mailbox The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] data The data associated to the message
 */
void dsend_message(const char * destination_mailbox, IPMessageType type, void * data = nullptr);

/**
 * @brief Receive a message on a given mailbox
 * @param[in] reception_mailbox The mailbox name
 * @return The received message. Must be deallocated by the caller.
 */
IPMessage * receive_message(const std::string & reception_mailbox);

/**
 * @brief Check if the mailbox is empty
 * @param[in] reception_mailbox The mailbox name
 * @return Boolean indicating if the mailbox is empty
 */
bool mailbox_empty(const std::string & reception_mailbox);

/**
 * @brief Transforms a IPMessageType into a std::string
 * @param[in] type The IPMessageType
 * @return The std::string corresponding to the type
 */
std::string ip_message_type_to_string(IPMessageType type);

/**
 * @brief Transforms a SumbitterType into a std::string
 * @param[in] type The SubmitterType
 * @return The std::string corresponding to the type
 */
std::string submitter_type_to_string(SubmitterType type);

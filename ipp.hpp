#pragma once

/**
 * @file ipp.hpp Inter-Process Protocol
 */

#include <vector>
#include <string>

#include <simgrid/msg.h>

struct BatsimContext;

enum class IPMessageType
{
    JOB_SUBMITTED       //!< Submitter -> Server. The submitter tells the server a new job has been submitted.
    ,JOB_COMPLETED      //!< Launcher/killer -> Server. The launcher tells the server a job has been completed.
    ,PSTATE_MODIFICATION//!< SchedulerHandler -> Server. The scheduler handler tells the server a scheduling event occured (a pstate modification).
    ,SCHED_ALLOCATION   //!< SchedulerHandler -> Server. The scheduler handler tells the server a scheduling event occured (a job allocation).
    ,SCHED_NOP          //!< SchedulerHandler -> Server. The scheduler handler tells the server a scheduling event occured (a NOP message).
    ,SCHED_READY        //!< SchedulerHandler -> Server. The scheduler handler tells the server that the scheduler is ready (messages can be sent to it).
    ,SUBMITTER_HELLO    //!< Submitter -> Server. The submitter tells it starts submitting to the server.
    ,SUBMITTER_BYE      //!< Submitter -> Server. The submitter tells it stops submitting to the server.
    ,SWITCHED_ON        //!< SwitcherON -> Server. The switcherON process tells the server the machine pstate has been changed
    ,SWITCHED_OFF       //!< SwitcherOFF -> Server. The switcherOFF process tells the server the machine pstate has been changed.
};

struct JobSubmittedMessage
{
    int job_id; //! The job ID
};

struct JobCompletedMessage
{
    int job_id; //! The job ID
};

struct SchedulingAllocation
{
    int job_id;
    std::vector<int> machine_ids;   //! The IDs of the machines on which the job should be allocated
    std::vector<msg_host_t> hosts;  //! The corresponding SimGrid hosts
};

struct SchedulingAllocationMessage
{
    std::vector<SchedulingAllocation> allocations;  //! Possibly several allocations
};

struct PStateModificationMessage
{
    int machine;
    int new_pstate;
};

struct IPMessage
{
    ~IPMessage();
    IPMessageType type; //! The message type
    void * data;        //! The message data (can be NULL if type is in [SCHED_NOP, SUBMITTER_HELLO, SUBMITTER_BYE, SUBMITTER_READY]). Otherwise, it is either a JobSubmittedMessage*, a JobCompletedMessage* or a SchedulingAllocationMessage* according to type.
};

struct RequestReplyProcessArguments
{
    BatsimContext * context;
    std::string send_buffer;
};

struct ServerProcessArguments
{
    BatsimContext * context;
};

struct ExecuteJobProcessArguments
{
    BatsimContext * context;
    SchedulingAllocation allocation;
};

struct KillerProcessArguments
{
    msg_task_t task; //! The task that will be cancelled if the walltime is reached
    double walltime; //! The number of seconds to wait before cancelling the task
};

struct SwitchPStateProcessArguments
{
    BatsimContext * context;
    PStateModificationMessage * message;
};

struct JobSubmitterProcessArguments
{
    BatsimContext * context;
};

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] dst The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] job_id The job the message is about
 * @param[in] data The data associated to the message
 */
void send_message(const std::string & destination_mailbox, IPMessageType type, void * data = nullptr);
void send_message(const char * destination_mailbox, IPMessageType type, void * data = nullptr);

std::string ipMessageTypeToString(IPMessageType type);

#pragma once

#include <simgrid/msg.h>

/**
 * @brief Types of tasks exchanged between nodes.
 */
typedef enum
{
    FINALIZE				//!< Server -> Node. The server tells the node to stop its execution.
    ,LAUNCH_JOB				//!< Server -> Node. The server tells the node to launch a new job.
    ,JOB_SUBMITTED			//!< Submitter -> Server. The submitter tells the server a new job has been submitted.
    ,JOB_COMPLETED			//!< Launcher/killer -> Server. The launcher or killer tells the server a job has been completed.
    ,CHANGE_PSTATE          //!< Server -> PstateChanger. The server tells the pstate changer that the machine pstate must be changed.
    ,MACHINE_PSTATE_CHANGED //!< PstateChanger -> Server. The changer tells the server the machine pstate has been changed.
    ,SCHED_EVENT			//!< SchedulerHandler -> Server. The scheduler handler tells the server a scheduling event occured.
    ,SCHED_READY			//!< SchedulerHandler -> Server. The scheduler handler tells the server that the scheduler is ready (messages can be sent to it).
    ,SUBMITTER_HELLO		//!< Submitter -> Server. The submitter tells it starts submitting to the server.
    ,SUBMITTER_BYE			//!< Submitter -> Server. The submitter tells it stops submitting to the server.
} e_task_type_t;

/**
 * @brief Data attached with the tasks used to communicate between MSG processes
 */
typedef struct s_task_data
{
    e_task_type_t type;	//! Type of task
    int job_id;			//! The job ID
    void *data;			//! Either NULL or points to something else based on type
} s_task_data_t;

/**
 * @brief Data structure used to launch a job
 */
typedef struct s_lauch_data
{
    int jobID;                       	//! The job identification number
    int reservedNodeCount;           	//! The number of reserved nodes
    int * reservedNodesIDs;          	//! The nodes on which the job will be run
} s_launch_data_t;

typedef struct s_change_pstate_data
{
    int machineID;  //! The machine identification number
    int pstate;     //! The pstate in which the machine must be set
} s_change_pstate_data_t;

void send_message(const char *dst, e_task_type_t type, int job_id, void *data);

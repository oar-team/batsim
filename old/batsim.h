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
    ,SCHED_EVENT			//!< SchedulerHandler -> Server. The scheduler handler tells the server a scheduling event occured.
    ,SCHED_READY			//!< SchedulerHandler -> Server. The scheduler handler tells the server that the scheduler is ready (messages can be sent to it).
    ,LAUNCHER_INFORMATION	//!< Node -> Launcher. The node gives launching information to the job launcher.
    ,KILLER_INFORMATION		//!< Node -> Killer. The node gives killing information to the job killer.
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

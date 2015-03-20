#pragma once

#include <msg/msg.h>

/**
 * @brief Types of tasks exchanged between nodes.
 */
typedef enum
{
	FINALIZE				//! Server -> Node
	,LAUNCH_JOB				//! Server -> Node
	,JOB_SUBMITTED			//! Submitter -> Server
	,JOB_COMPLETED			//! Launcher/killer -> Server
	,SCHED_EVENT			//! SchedulerHandler -> Server
	,SCHED_READY			//! SchedulerHandler -> Server
	,LAUNCHER_INFORMATION	//! Node -> Launcher
	,KILLER_INFORMATION		//! Node -> Killer
	,SUBMITTER_HELLO		//! Submitter -> Server
	,SUBMITTER_BYE			//! Submitter -> Server
} e_task_type_t;

/*
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
	msg_process_t killerProcess;		//! The SG killer process
	struct s_kill_data * killerData; 	//! The data used by the killer
	xbt_dict_t dataToRelease;			//! The data used by the launcher which will need to be released (used when killing the job)
} s_launch_data_t;

/**
 * @brief Data structure used to kill a job
 */
typedef struct s_kill_data
{
	msg_process_t launcherProcess;	//! The SG launcher process
	s_launch_data_t * launcherData;	//! The data used by the launcher
} s_kill_data_t;

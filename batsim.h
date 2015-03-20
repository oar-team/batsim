#pragma once

#include <msg/msg.h>

/**
 * Types of tasks exchanged between nodes.
 */
typedef enum
{
	ECHO,
	FINALIZE,
	LAUNCH_JOB,
	JOB_SUBMITTED,
	JOB_COMPLETED,
	KILL_JOB,
	SUSPEND_JOB,
	SCHED_READY,
	LAUNCHER_INFORMATION,
	KILLER_INFORMATION
} e_task_type_t;

/*
 * Data attached with the tasks sent and received
 */
typedef struct s_task_data
{
	e_task_type_t type;                     // type of task
	int job_id;
	void *data;
	const char* src;           // used for logging
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

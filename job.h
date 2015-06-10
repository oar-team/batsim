/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */
#pragma once

#include <sys/types.h> /* ssize_t, needed by xbt/str.h, included by msg/msg.h */

#include <simgrid/msg.h>
#include <xbt.h>
#include <xbt/sysdep.h>
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <jansson.h> /* json parsing */


// Job definition

//! Enumerates the job states
typedef enum e_job_state
{
     JOB_STATE_NOT_SUBMITTED          //!< The job exists but cannot be scheduled yet
    ,JOB_STATE_SUBMITTED              //!< The job has been submitted, it can now be scheduled
    ,JOB_STATE_RUNNING                //!< The job has been scheduled and is currently being processed
    ,JOB_STATE_COMPLETED_SUCCESSFULLY //!< The job execution finished before its walltime
    ,JOB_STATE_COMPLETED_KILLED       //!< The job execution time was longer than its walltime so the job had been killed
} e_job_state_t;

//! The structure used to store a job
typedef struct s_job
{
    int id;                 //! The job ID (as integer)
    char *id_str;           //! The job ID (as string)
    char *profile;          //! The job profile
    double submission_time; //! The time at which the job is available to be scheduled
    double walltime;        //! The maximum execution time autorized for the job
    int nb_res;             //! The number of resources the job asked for

    double startingTime;    //! The time at which the job started being executed
    double runtime;         //! The execution time of the job
    int * alloc_ids;        //! The resources allocated that were allocated to this job
    e_job_state_t state;    //! The state
} s_job_t;


// Profiles definition

typedef enum e_IO_type
{
    READ,
    WRITE
} e_IO_type_t;

//! The structure used to store additional information about profiles of type msg_par
typedef struct s_msg_par
{
    int nb_res;     //! The number of resources
    double *cpu;    //! The computation vector
    double *com;    //! The communication matrix
} s_msg_par_t;

//! The structure used to store additional information about profiles of type msg_par_hg
typedef struct s_msg_par_hg
{
    double cpu;     //! The computation amount on each node
    double com;     //! The communication amount between each pair of nodes
} s_msg_par_hg_t;

//! The structure used to store additional information about profiles of type IO_store_and_forward
typedef struct s_IO_store_and_forward
{
    char **data_nodes; 		//! The data node host id list
    e_IO_type_t IOtype;		//! Type of IO from the data node (read or write)
    double *remote_IO_per_data_node;	//! The remote IO matrix between ressources in x and data nodes in y
} s_IO_read_store_and_forward_t;

//! The structure used to store additional information about profiles of type composed
typedef struct s_composed_prof
{
    int nb;     //! The number of times the sequence must be repeated
    int lg_seq; //! The sequence length
    char **seq; //! The sequence of profile names (array of char* of size lg_seq)
} s_composed_prof_t;

//! The structure used to store additional information about jobs of type delay
typedef struct s_delay
{
    double delay;
} s_delay_t;

/**
 * @brief Executes a job
 * @param[in] job_id The job number
 * @param[in] nb_res The number of resources on which the job will be executed
 * @param[in] res_idxs The resources on which the job will be executed (indexes of the nodes parameter)
 * @param[in] nodes The array of hosts which can be used to execute the job
 * @param[in] walltime The maximum execution time of the job
 * @return 1 if the job finished in time, 0 if the walltime had been reached
 */
int job_exec(int job_id, int nb_res, int *res_idxs, msg_host_t *nodes, double walltime);

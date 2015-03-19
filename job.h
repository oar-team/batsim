/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */
#pragma once

#include <sys/types.h> /* ssize_t, needed by xbt/str.h, included by msg/msg.h */

#include <msg/msg.h>
#include <xbt.h>
#include <xbt/sysdep.h>
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <jansson.h> /* json parsing */

//XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "Batsim");

typedef enum e_job_state
{
     JOB_STATE_NOT_SUBMITTED          //! The job exists but cannot be scheduled yet
    ,JOB_STATE_SUBMITTED              //! The job has been submitted, it can now be scheduled
    ,JOB_STATE_RUNNING                //! The job has been scheduled and is currently being processed
    ,JOB_STATE_COMPLETED_SUCCESSFULLY //! The job execution finished before its walltime
    ,JOB_STATE_COMPLETED_KILLED       //! The job execution time was longer than its walltime so the job had been killed
} e_job_state_t;

typedef struct s_job
{
    int id;                 //! The job ID (as integer)
    char *id_str;           //! The job ID (as string)
    char *profile;          //! The job profile
    double submission_time; //! The time at which the job is available to be scheduled
    double walltime;        //! The maximum execution time autorized for the job
    double runtime;         //! The execution time of the job
    int nb_res;             //! The number of resources the job asked for
    e_job_state_t state;    //! The state
} s_job_t;

typedef struct s_msg_par
{
    int nb_res;     //! The number of resources
    double *cpu;    //! The computation vector
    double *com;    //! The communication matrix
} s_msg_par_t;

typedef struct s_msg_par_hg
{
    double cpu;     //! The computation amount on each node
    double com;     //! The communication amount between each pair of nodes
} s_msg_par_hg_t;

typedef struct s_composed_prof
{
    int nb;     //! The number of times the sequence must be repeated
    int lg_seq; //! The sequence length
    char **seq; //! The sequence of profile names (array of char* of size lg_seq)
} s_composed_prof_t;

typedef struct s_delay
{
    double delay;
} s_delay_t;

void job_exec(int job_id, int nb_res, int *res_idxs, msg_host_t *nodes, xbt_dict_t * allocatedStuff);

void freeTask(void * task);
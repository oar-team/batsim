/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */ 
#ifndef BATSIM_JOB_H
#define BATSIM_JOB_H

#include "msg/msg.h" 
#include "xbt.h"
#include "xbt/sysdep.h"
#include "xbt/log.h"
#include "xbt/asserts.h"

#include <jansson.h> /* json parsing */

//XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "Batsim");

typedef struct s_job {
  int id;
  char *id_str;
  const char *profile;
  double submission_time;
  double walltime;
  double runtime;
  int nb_res;
} s_job_t, *job_t;

typedef struct s_msg_par {
  int nb_res;
  double *cpu;
  double *com;
} s_msg_par_t, *msg_par_t;

typedef struct s_msg_par_hg {
  double cpu;
  double com;
} s_msg_par_hg_t, *msg_par_hg_t;

typedef struct s_composed_prof {
  int nb;
  int lg_seq;
  int *seq;
} s_composed_prof_t, *composed_prof_t;

typedef struct s_delay {
  double delay;
} s_delay_t, *delay_t;

void job_exec(int job_idx, int *res_idxs, msg_host_t *nodes);

#endif                          /* BATSIM_JOB_H */

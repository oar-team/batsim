/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(job, "job");

void profile_exec(const char *profile_str, int nb_res, msg_host_t *job_res) {

  profile_t profile;
  double *computation_amount;
  double *communication_amount;
  msg_task_t ptask;
  int i, j;

  profile = xbt_dict_get(profiles, profile_str);

  if (strcmp(profile->type, "msg_par") == 0) {
    computation_amount = malloc(nb_res * sizeof(double));
    communication_amount = malloc(nb_res * nb_res * sizeof(double));

    double *cpu = ((msg_par_t)(profile->data))->cpu;
    double *com = ((msg_par_t)(profile->data))->com;

    memcpy(computation_amount , cpu, nb_res * sizeof(double));
    memcpy(communication_amount, com, nb_res * nb_res * sizeof(double));

    ptask = MSG_parallel_task_create("parallel task",
				     nb_res, job_res,
				     computation_amount,
				     communication_amount, NULL);
    MSG_parallel_task_execute(ptask);

    MSG_task_destroy(ptask);

  } else if (strcmp(profile->type, "msg_par_hg") == 0) {

    double cpu = ((msg_par_hg_t)(profile->data))->cpu;
    double com = ((msg_par_hg_t)(profile->data))->com;

    computation_amount = malloc(nb_res * sizeof(double));
    communication_amount = malloc(nb_res * nb_res * sizeof(double));

    XBT_DEBUG("msg_par_hg: nb_res: %d , cpu: %f , com: %f", nb_res, cpu, com);
    
    for (i = 0; i < nb_res; i++) {
      computation_amount[i] = cpu;
    }
    
    for (j = 0; j < nb_res; j++) {
      for (i = 0; i < nb_res; i++) {
	communication_amount[nb_res * j + i] = com;
      }
    }
    
    ptask = MSG_parallel_task_create("parallel task hg",
				     nb_res, job_res,
				     computation_amount,
				     communication_amount, NULL);
    MSG_parallel_task_execute(ptask);

    MSG_task_destroy(ptask);

  } else if (strcmp(profile->type, "composed") == 0) {

    char buffer[20];

    int nb = ((composed_prof_t)(profile->data))->nb;
    int lg_seq = ((composed_prof_t)(profile->data))->lg_seq;
    int *seq = ((composed_prof_t)(profile->data))->seq;

    XBT_DEBUG("composed: nb: %d, lg_seq: %d", nb, lg_seq);

    for(j = 0; j < lg_seq; j++) {
      for(i = 0; i < nb; i++) {
	sprintf(buffer, "%d", seq[j]);
	profile_exec(buffer, nb_res, job_res);
      }
    }

  } else if (strcmp(profile->type, "delay") == 0) {

    double delay = ((delay_t)(profile->data))->delay;
    MSG_process_sleep(delay);

  } else if (strcmp(profile->type, "smpi") == 0) {

    xbt_die("Profile with type %s is not yet implemented", profile->type);

  } else {

    xbt_die("Profile with type %s is not supported", profile->type);

  }
  
}

/**
 * \brief Load workload with jobs' profiles file
 */

void job_exec(int job_idx, int  *res_idxs, msg_host_t *nodes) {

  msg_host_t *job_res;
  s_job_t job;
  int nb_res;
  int i;

  job = jobs[job_idx]; 
 
  XBT_DEBUG("Launch_job: idx %d, id %s profile %s", job_idx, jobs[job_idx].id_str, job.profile);

  nb_res = job.nb_res;

  job_res = (msg_host_t *)malloc( nb_res * sizeof(s_msg_host_t) );

  for(i = 0; i < nb_res; i++) {
    job_res[i] = nodes[res_idxs[i]];
 
  }

  profile_exec(job.profile, nb_res, job_res);

  free(job_res);
}


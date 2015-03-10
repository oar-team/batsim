/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */ 
#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(job, "job");

/**
 * \brief Load workload with jobs' profiles file
 */

void job_exec(int job_idx, int  *res_idxs, msg_host_t *nodes) {

  double *computation_amount;
  double *communication_amount;
  msg_host_t *job_res;
  char *res;
  msg_task_t ptask;
  s_job_t job;
  int i, j;
  int nb_res;
  profile_t profile;

  job = jobs[job_idx]; 
  profile = xbt_dict_get(profiles, job.profile);

  XBT_DEBUG("Launch_job: idx %d, id %s profile %s", job_idx, jobs[job_idx].id_str, job.profile);

  if (strcmp(profile->type, "msg_par") == 0) {
    
    nb_res = ((msg_par_t)(profile->data))->nb_res;

    computation_amount = malloc(nb_res * sizeof(double));
    communication_amount = malloc(nb_res * nb_res * sizeof(double));

    double *cpu = ((msg_par_t)(profile->data))->cpu;
    double *com = ((msg_par_t)(profile->data))->com;

    memcpy(computation_amount , cpu, nb_res * sizeof(double));
    memcpy(communication_amount, com, nb_res * nb_res * sizeof(double));
    
    //computation_amount = ((msg_par_t)(profile->data))->cpu;
    //communication_amount = ((msg_par_t)(profile->data))->com;
    //printf("nb_res %d cpu %f com %f \n", nb_res, cpu[0], com[0]);

    job_res = (msg_host_t *)malloc( nb_res * sizeof(s_msg_host_t) );
    for(i = 0; i < nb_res; i++) {
      job_res[i] = nodes[res_idxs[i]];
    }

    ptask = MSG_parallel_task_create("parallel task",
				     nb_res, job_res,
				     computation_amount,
				     communication_amount, NULL);
    MSG_parallel_task_execute(ptask);

    //free(job_res);

    MSG_task_destroy(ptask);

  } else {
    xbt_die("Type %s of profile is not supported", profile->type);
  }
  
  /* There is no need to free that! why ???*/
  /*   free(communication_amount); */
  /*   free(computation_amount); */

}

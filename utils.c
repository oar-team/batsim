/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */ 
#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(utils, "utils");

/**
 * \brief Load workload with jobs' profiles file
 */

int nb_jobs = 0;

json_t *load_json_workload_profile(char *filename) {
  json_t *root;
  json_error_t error;
  json_t *e;

  if (filename == NULL) {
    filename = "../workload_profiles/test_workload_profile.json";
  }

  root = json_load_file(filename, 0, &error);

  if(!root){
    XBT_ERROR("error : root\n");
    XBT_ERROR("error : on line %d: %s\n", error.line, error.text);
    exit(1);
  } else {

    e = json_object_get(root, "description");
    if ( e != NULL ) {
      XBT_INFO("Json Profile and Workload File's description: \n%s", json_string_value(e)); 
    }
    return root;
  }
}

double json_number_to_double(json_t *e) {
  double value;
  if (json_typeof(e) == JSON_INTEGER) {
    value = (double)json_integer_value(e);
  } else {
    value = (double)json_real_value(e);
  }
  return value;
}

void retrieve_jobs(json_t *root) {
  json_t *e;
  json_t *j;
  job_t job;
  int i = 0;
  int *idx;
  xbt_dynar_t jobs_dynar = xbt_dynar_new(sizeof(s_job_t), NULL);
  jobs_idx2id = xbt_dict_new();

  e = json_object_get(root, "jobs");
  if ( e != NULL ) {
    nb_jobs = json_array_size(e);
    XBT_INFO("Json Workload with Profile File: nb_jobs %d", nb_jobs);
    for(i=0; i<nb_jobs; i++) {
      job = (job_t)malloc( sizeof(s_job_t) );

      j = json_array_get(e, i);
      job->id = json_integer_value(json_object_get(j,"id"));
      job->id_str = (char *)malloc(10);
      snprintf(job->id_str, 10, "%d", job->id);

      job->submission_time = json_number_to_double(json_object_get(j,"subtime"));
      job->walltime = json_number_to_double(json_object_get(j,"walltime"));
      job->profile = json_string_value(json_object_get(j,"profile"));      
      job->runtime = -1;
      job->nb_res = json_number_to_double(json_object_get(j,"res"));

      XBT_INFO("Job:: idx: %d, id: %s, subtime: %f, profile: %s, nb_res: %d",
	       i, job->id_str, job->submission_time, job->profile, job->nb_res);
      
      xbt_dynar_push(jobs_dynar, job);
      
      idx = (int *)malloc(sizeof(int));
      *idx = i; 
      xbt_dict_set(jobs_idx2id, job->id_str, idx, free);

    }

    jobs = xbt_dynar_to_array(jobs_dynar); 
    //xbt_dynar_free_container(&jobs_dynar);
  } else {
    XBT_INFO("Json Workload with Profile File: jobs array is missing !");
    exit(1);
  }

}

void retrieve_profiles(json_t *root) {
  json_t *j_profiles;
  const char *key;
  json_t *j_profile;
  json_t *e;
  const char *type;
  int nb_res = 0;
  double *cpu;
  double *com;
  int i = 0;
  msg_par_t m_par;
  profile_t profile;

  j_profiles = json_object_get(root, "profiles");
  if ( j_profiles != NULL ) {

    profiles = xbt_dict_new();

    void *iter = json_object_iter(j_profiles);
    while(iter) {
      
      key = json_object_iter_key(iter);
      j_profile = json_object_iter_value(iter);
   
      type = json_string_value( json_object_get(j_profile, "type") );

      if (strcmp(type, "msg_par") ==0) {
	e = json_object_get(j_profile, "cpu");
	nb_res = json_array_size(e);
	cpu = xbt_new0(double, nb_res);
	com = xbt_new0(double, nb_res * nb_res);
	for(i=0; i < nb_res; i++) 
	  cpu[i] = (double)json_number_to_double( json_array_get(e,i) );

	e = json_object_get(j_profile, "com");
	for(i=0; i < nb_res * nb_res; i++) 
	  com[i] = (double)json_number_to_double( json_array_get(e,i) );
	
	m_par = (msg_par_t)malloc( sizeof(s_msg_par_t) ); 
	profile = (profile_t)malloc( sizeof(s_profile_t) ); 

	m_par->nb_res = nb_res;
	m_par->cpu = cpu;
	m_par->com = com;

	profile->type = type;
	profile->data = m_par;
	
	xbt_dict_set(profiles, key, profile, free);

      }
      
      iter = json_object_iter_next(j_profiles, iter);
    }

  } else {
    XBT_INFO("Json Workload with Profile File: profiles dict is missing !");
    exit(1);
  }

}

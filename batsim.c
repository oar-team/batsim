/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

#include <jansson.h> /* json parsing */

#include "msg/msg.h"            
#include "xbt/sysdep.h"         /* calloc, printf */

/* Create a log channel to have nice outputs. */
#include "xbt/log.h"
#include "xbt/asserts.h"
XBT_LOG_NEW_DEFAULT_CATEGORY(batsim,
                             "Messages specific for this msg example");

#define BAT_SOCK_NAME "/tmp/bat_socket"

#define COMM_SIZE 0.000001
//#define COMM_SIZE 0.0 // => raise core dump 
#define COMP_SIZE 0.0

#define true 1
#define false 0

/**
 * Types of tasks exchanged between nodes.
 */
typedef enum {
  ECHO,
  FINALIZE,
  LAUNCH_JOB,
  JOB_SUBMITTED,
  JOB_COMPLETED,
  KILL_JOB,
  SUSPEND_JOB,
  SCHED_READY
} e_task_type_t;

char *task_type2str[] = {
  "ECHO",
  "FINALIZE",
  "LAUNCH_JOB",
  "JOB_SUMITTED",
  "JOB_COMPLETED",
  "KILL_JOB",
  "SUSPEND_JOB",
  "SCHED_READY"
};

/*
 * Data attached with the tasks sent and received
 */
typedef struct s_task_data {
  e_task_type_t type;                     // type of task
  int job_idx;
  void *data;
  const char* src;           // used for logging
} s_task_data_t, *task_data_t;

typedef struct s_job {
  int id;
  char *id_str;
  const char *profile;
  double submission_time;
  double walltime;
  double runtime;
} s_job_t, *job_t;

typedef struct s_msg_par {
  int nb_res;
  double *cpu;
  double *com;
} s_msg_par_t, *msg_par_t;

typedef struct s_profile {
  const char *type;
  void *data;
} s_profile_t, *profile_t;

int nb_nodes = 0;
msg_host_t *nodes;

int nb_jobs = 0;
s_job_t *jobs;

xbt_dict_t profiles;
xbt_dict_t jobs_idx2id;
int uds_fd = -1; 

// process functions
static int node(int argc, char *argv[]);
static int server(int argc, char *argv[]);
static int launch_job(int argc, char *argv[]);
static void send_message(const char *dst, e_task_type_t type, int job_id, void *data);

/** 
 * \brief Send message through Unix Domain Socket
 */                                                  
static int send_uds(char *msg) 
{ 
  int32_t lg = strlen(msg);
  XBT_INFO("send_uds, lg: %d", lg);
  write(uds_fd, &lg, 4);
  write(uds_fd, msg, lg);
  free(msg);
}

/** 
 * \brief Receive message through Unix Domain Socket
 */

static char *recv_uds()
{
  int32_t lg;
  char *msg;
  read(uds_fd, &lg, 4);
  printf("msg size to recv %d\n", lg);
  msg = (char *) malloc(sizeof(char)*(lg+1)); /* 1 for null terminator */
  read(uds_fd, msg, lg);
  msg[lg] = 0;
  printf("msg: %s\n", msg);

  return msg;  
}

/** 
 * \brief Open Unix Domain Socket for communication with scheduler
 */
static void open_uds() 
{
  struct sockaddr_un address;

  uds_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(uds_fd < 0) {
    XBT_ERROR("socket() failed\n");
    exit(1);
  }
  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, 255, BAT_SOCK_NAME);

  if(connect(uds_fd, 
	     (struct sockaddr *) &address, 
	     sizeof(struct sockaddr_un)) != 0)
    {
      XBT_ERROR("connect() failed\n");
      exit(1);
    }
}

/** 
 * \brief 
 */
static int send_sched(int argc, char *argv[])
{
  double t_send = MSG_get_clock();
  double t_answer = 0;
  
  char *message_recv = NULL;
  char *message_send = MSG_process_get_data(MSG_process_self());

  xbt_dynar_t answer_dynar;

  send_uds(message_send);
  message_recv = recv_uds();
  
  answer_dynar = xbt_str_split(message_recv, ":");	

  t_answer = atof( *(char **)xbt_dynar_get_ptr(answer_dynar, 1) ); 

  //waiting before consider the sched's answer
  MSG_process_sleep(t_answer - t_send);
  XBT_INFO("send_sched, msg type %p", (char **)xbt_dynar_get_ptr(answer_dynar, 2));
  //signal 
  send_message("server", SCHED_READY, 0, &answer_dynar);

  free(message_recv);
  return 0;
}

/** 
 * \brief
 */
void send_message(const char *dst, e_task_type_t type, int job_idx, void *data)
{
  msg_task_t task_sent;
  task_data_t req_data = xbt_new0(s_task_data_t,1);
  req_data->type = type;
  req_data->job_idx = job_idx;
  req_data->data = data;
  req_data->src =  MSG_host_get_name(MSG_host_self());
  task_sent = MSG_task_create(NULL, COMP_SIZE, COMM_SIZE, req_data);

  XBT_INFO("data pointer %p", req_data->data);

  MSG_task_send(task_sent, dst);
}

/** 
 * \brief
 */
static void task_free(struct msg_task ** task)
{
  if(*task != NULL){
    xbt_free(MSG_task_get_data(*task));
    MSG_task_destroy(*task);
    *task = NULL;
  }
}


/** 
 * \brief 
 */
static int launch_job(int argc, char *argv[])
{
  //double task_comp_size = 1000000;
  //double task_comm_size = 10000;
  double *computation_amount;
  double *communication_amount;
  msg_host_t *job_res;
  char *res;
  msg_task_t ptask;
  s_job_t job;
  int i, j;
  int nb_res;
  profile_t profile;

  task_data_t task_data = MSG_process_get_data(MSG_process_self());

  int job_idx = task_data->job_idx;

  xbt_dynar_t res_dynar = (xbt_dynar_t)(task_data->data);

  XBT_INFO("head node id %s", *(char **)xbt_dynar_get_ptr(res_dynar, 0));
  
  XBT_DEBUG("Launch_job: idx %d, id %s profile %s", job_idx, jobs[job_idx].id_str, job.profile);
  
  job = jobs[job_idx]; 

  profile = xbt_dict_get(profiles, job.profile);
  
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
    xbt_dynar_foreach(res_dynar, i, res){
      job_res[i] = nodes[atoi(res)];
    }

    ptask = MSG_parallel_task_create("parallel task",
				     nb_res, job_res,
				     computation_amount,
				     communication_amount, NULL);
    MSG_parallel_task_execute(ptask);

    MSG_task_destroy(ptask);

  } else {
    xbt_die("Type %s of profile is not supported", profile->type);
  }
  

  /*

  computation_amount = xbt_new0(double, nb_nodes);
  communication_amount = xbt_new0(double, nb_nodes * nb_nodes);
  
  task_comp_size = task_comp_size + rand() % 5000000;

  for (i = 0; i < nb_nodes; i++)
    computation_amount[i] = task_comp_size;

  for (i = 0; i < nb_nodes; i++)
    for (j = i + 1; j < nb_nodes; j++)
      communication_amount[i * nb_nodes + j] = task_comm_size;
  //nodes_count = 2;
  //MSG_process_sleep(rand() % 1000);
  ptask = MSG_parallel_task_create("parallel task",
                                   nb_nodes, nodes,
                                   computation_amount,
                                   communication_amount, NULL);
  MSG_parallel_task_execute(ptask);

  MSG_task_destroy(ptask);
  
  */

  xbt_dynar_free(&res_dynar);
 
  send_message("server", JOB_COMPLETED, job_idx, NULL);

  /* There is no need to free that! why ???*/
  /*   free(communication_amount); */
  /*   free(computation_amount); */
  //free(nodes);

  return 0;
}



/** 
 * \brief 
 */
static int node(int argc, char *argv[]) 
{
  const char *node_id = MSG_host_get_name(MSG_host_self());

  msg_task_t task_received = NULL;
  task_data_t task_data;
  int type = -1;

  XBT_INFO("I am %s", node_id);
  
  while(1) {
    MSG_task_receive(&(task_received), node_id);
    
    task_data = (task_data_t) MSG_task_get_data(task_received);
    type = task_data->type;

    XBT_INFO("MSG_Task received %s, type %s", node_id, task_type2str[type]);
    
    if (type == FINALIZE) break;

    switch (type) {
    case LAUNCH_JOB:
      MSG_process_create("launch_job", launch_job, task_data, MSG_host_self());
      break;
    }
    task_free(&task_received);
  }

}

/** 
 * \brief Unroll jobs submission
 * 
 * Jobs' submission is managed by a dedicate MSG_process
 */
static int jobs_submitter(int argc, char *argv[]) {

  double submission_time = 0.0;
  int job2submit_idx = 0;
  xbt_dynar_t jobs2sub_dynar;
  double t = MSG_get_clock();
  int job_idx, i;

  while (job2submit_idx < nb_jobs) {
    submission_time = jobs[job2submit_idx].submission_time;

    jobs2sub_dynar = xbt_dynar_new(sizeof(int), NULL);
    while(submission_time == jobs[job2submit_idx].submission_time) {
      xbt_dynar_push_as(jobs2sub_dynar, int, job2submit_idx);
      XBT_INFO("job2submit_idx %d, job_id %d", job2submit_idx, jobs[job2submit_idx].id);
      job2submit_idx++;
      if (job2submit_idx == nb_jobs) break;
    }
    
    MSG_process_sleep(submission_time - t);
    t = MSG_get_clock();

    xbt_dynar_foreach(jobs2sub_dynar, i, job_idx) { 
      send_message("server", JOB_SUBMITTED, job_idx, NULL);
    }
    xbt_dynar_free(&jobs2sub_dynar);
  }
  return 0;
}


/** 
 * \brief The central process which manage the global orchestration  
 */
int server(int argc, char *argv[]) {

  msg_host_t node;
  
  msg_task_t task_received = NULL;
  task_data_t task_data;

  int nb_completed_jobs = 0;
  int nb_submitted_jobs = 0;
  int sched_ready = true;
  char *sched_message = "";
  char *tmp;
  char *job_ready_str;
  char *jobid_ready;
  char *job_id_str;
  int size_m = 0;
  double t = 0;
  int i;

  while ( (nb_completed_jobs < nb_jobs) || !sched_ready ) {

    // wait message node, submitter, scheduler...
    MSG_task_receive(&(task_received), "server");

    task_data = (task_data_t) MSG_task_get_data(task_received);

    XBT_INFO("Server receive Task/message type %s:", task_type2str[task_data->type]);

    switch (task_data->type) {
    case JOB_COMPLETED:
      nb_completed_jobs++;
      job_id_str= jobs[task_data->job_idx].id_str;
      XBT_INFO("Job id %s COMPLETED, %d jobs completed", job_id_str, nb_completed_jobs);
      size_m = asprintf(&sched_message, "%s0:%f:C:%s|", sched_message, MSG_get_clock(), job_id_str);
      XBT_INFO("sched_message: %s", sched_message);

      //TODO add job_id + msg to send
      break;
    case JOB_SUBMITTED:
      nb_submitted_jobs++;
      job_id_str = jobs[task_data->job_idx].id_str;
      XBT_INFO("Job id %s SUBMITTED, %d jobs submitted", job_id_str, nb_submitted_jobs);
      size_m = asprintf(&sched_message, "%s0:%f:S:%s|", sched_message, MSG_get_clock(), job_id_str);
      XBT_INFO("sched_message: %s", sched_message);

      break;
    case SCHED_READY:
      //process scheduler message
      //up to now only jobs to launch
      // 0:timestamp:J:jid1=0,1,3;jid2=5,7,8,9
      // 0:        1:2:3

    
      XBT_DEBUG("Pointer %p", (char **)xbt_dynar_get_ptr( *( (xbt_dynar_t *)task_data->data), 2 ));

      tmp = *(char **)xbt_dynar_get_ptr( *( (xbt_dynar_t *)task_data->data), 2 );

      XBT_INFO("type of receive message from scheduler: %c", *tmp);
      switch (*tmp) {
      case 'J':
	tmp = *(char **)xbt_dynar_get_ptr( *( (xbt_dynar_t *)task_data->data), 3 );
	xbt_dynar_t jobs_ready_dynar = xbt_str_split(tmp, ";");
	
       	xbt_dynar_foreach(jobs_ready_dynar, i, job_ready_str) {
	  if (job_ready_str != NULL) {
	    XBT_INFO("job_ready: %s", job_ready_str);
	    xbt_dynar_t job_id_res = xbt_str_split(tmp, "=");
	    job_id_str = *(char **)xbt_dynar_get_ptr(job_id_res, 0);
	    
	    int *job_idx =  xbt_dict_get(jobs_idx2id, job_id_str);
	 
	    xbt_dynar_t res_dynar = xbt_str_split(*(char **)xbt_dynar_get_ptr(job_id_res, 1), ",");

	    XBT_INFO("YOP %p", res_dynar );
 
	    int head_node = atoi(*(char **)xbt_dynar_get_ptr(res_dynar, 0));
	    XBT_INFO("head node: %s, id: %d", MSG_host_get_name(nodes[head_node]), head_node);
	 
	    send_message(MSG_host_get_name(nodes[head_node]), LAUNCH_JOB, *job_idx, res_dynar);

	    xbt_dynar_free(&job_id_res); 
	  }
	}
	  
	xbt_dynar_free(&jobs_ready_dynar);
	break;

      case 'N':
	XBT_DEBUG("Nothing to do");
	break;

      default:
	XBT_ERROR("Command %s is not supported", tmp); 
	break;
      }
      
      // why this raises a core dump ???
      //xbt_dynar_free( (xbt_dynar_t *)task_data->data );

      sched_ready = true;

      break;
    }

    task_free(&task_received);

    if ( sched_ready && (strcmp(sched_message, "") !=0) ) {
      //add current time to sched_message 
      size_m = asprintf(&sched_message, "%s0:%f:T", sched_message, MSG_get_clock());
      MSG_process_create("send message to sched", send_sched, sched_message, MSG_host_self() );
      sched_message = "";
      sched_ready = false;
    }


  }

  //send finalize to node
  XBT_INFO("All jobs completed, time to finalize");
       
  for(i=0; i < nb_nodes; i++) {
    send_message(MSG_host_get_name(nodes[i]), FINALIZE, 0, NULL);
  }

}

/** 
 * \brief 
 */
msg_error_t deploy_all(const char *platform_file)
{
  msg_error_t res = MSG_OK;
  xbt_dynar_t all_hosts;
  msg_host_t first_host;
  msg_host_t host;
  int i;

  MSG_config("workstation/model", "ptask_L07");
  MSG_create_environment(platform_file);

  all_hosts = MSG_hosts_as_dynar();
  xbt_dynar_get_cpy(all_hosts, 0, &first_host);
  //first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);
  xbt_dynar_remove_at(all_hosts, 0, NULL);

  xbt_dynar_foreach(all_hosts, i, host) {
      XBT_INFO("Create node process %d !", i);
      MSG_process_create("node", node, NULL, host);
  }

  
  nb_nodes = xbt_dynar_length(all_hosts);
  nodes = xbt_dynar_to_array(all_hosts);
  //xbt_dynar_free(&all_hosts);

  XBT_INFO("Nb nodes: %d", nb_nodes);

  MSG_process_create("server", server, NULL, first_host);
  MSG_process_create("jobs_submitter", jobs_submitter, NULL, first_host);
    
  res = MSG_main();
  

  XBT_INFO("Simulation time %g", MSG_get_clock());
  return res;
}


/**
 * \brief Load jobs profile and workload
 */
json_t *load_json_profile(char *filename) {
  json_t *root;
  json_error_t error;
  json_t *e;

  if (filename == NULL) {
    filename = "./profiles/test_profile.json";
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
    XBT_INFO("Json Profile and Workload File: nb_jobs %d", nb_jobs);
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
   
      XBT_INFO("Job:: idx: %d, id: %s, subtime: %f, profile: %s",
	       i, job->id_str, job->submission_time, job->profile);
      
      xbt_dynar_push(jobs_dynar, job);
      
      idx = (int *)malloc(sizeof(int));
      *idx = i; 
      xbt_dict_set(jobs_idx2id, job->id_str, idx, free);

    }

    jobs = xbt_dynar_to_array(jobs_dynar); 
    //xbt_dynar_free_container(&jobs_dynar);
  } else {
    XBT_INFO("Json Profile and Workload File: jobs array is missing !");
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

  printf("POYPOY\n");

  j_profiles = json_object_get(root, "profiles");
  if ( j_profiles != NULL ) {

    profiles = xbt_dict_new();

    printf("DICT\n");

    void *iter = json_object_iter(j_profiles);
    while(iter) {
      
      key = json_object_iter_key(iter);
      j_profile = json_object_iter_value(iter);
   
      type = json_string_value( json_object_get(j_profile, "type") );

      printf("KEY %s, profile type %s\n", key, type);


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
    XBT_INFO("Json Profile and Workload File: profiles dict is missing !");
    exit(1);
  }

}


int main(int argc, char *argv[])
{
  msg_error_t res = MSG_OK;
  int i;

  json_t *json_profile_workload;

  //Comment to remove debug message
  xbt_log_control_set("batsim.threshold:debug");

  json_profile_workload = load_json_profile(NULL);
  retrieve_jobs(json_profile_workload);
  retrieve_profiles(json_profile_workload);

  open_uds();

  MSG_init(&argc, argv);
  if (argc < 2) {
    printf("Usage: %s platform_file\n", argv[0]);
    printf("example: %s msg_platform.xml\n", argv[0]);
    exit(1);
  }


  /*                        */
  /* TODO load job workload */
  /*                        */  

  /* generate fake job workload
  nb_jobs = 3;
  jobs = malloc(nb_jobs * sizeof(s_job_t));

  for(i=0; i<nb_jobs; i++) {
    jobs[i].id = i;
    jobs[i].profile = "nop";
    jobs[i].submission_time = 10.0 * (i+1);
    jobs[i].runtime = -1;
  }

  */

  res = deploy_all(argv[1]);

  if (res == MSG_OK)
    return 0;
  else
    return 1;
}

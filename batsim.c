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

#include "msg/msg.h" 

#include "xbt/sysdep.h"         /* calloc, printf */
/* Create a log channel to have nice outputs. */
#include "xbt/log.h"
#include "xbt/asserts.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "Batsim");

#include "job.h"
#include "utils.h"

#define BAT_SOCK_NAME "/tmp/bat_socket"

#define COMM_SIZE 0.000001 //#define COMM_SIZE 0.0 // => raise core dump 
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

int nb_nodes = 0;
msg_host_t *nodes;

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
  char *core_message = NULL;
  xbt_dynar_t core_message_dynar;

  int size_m;

  xbt_dynar_t answer_dynar;


  //add current time to message
  size_m = asprintf(&message_send, "0:%f%s", MSG_get_clock(), message_send );
  send_uds(message_send);

  message_recv = recv_uds();
  answer_dynar = xbt_str_split(message_recv, "|");
  
  t_answer = atof( * (char **)xbt_dynar_get_ptr(answer_dynar, 0) + 2 );//2 left shift to skip "0:" 

  //waiting before consider the sched's answer
  MSG_process_sleep(t_answer - t_send);

  //XBT_INFO("send_sched, msg type %p", (char **)xbt_dynar_get_ptr(answer_dynar, 1));
  //signal
  core_message = *(char **)xbt_dynar_get_ptr(answer_dynar, 1);
  core_message_dynar = xbt_str_split(core_message, ":");
  send_message("server", SCHED_READY, 0, &core_message_dynar);

  xbt_dynar_free(&answer_dynar);
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
  req_data->src = MSG_host_get_name(MSG_host_self());
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

  task_data_t task_data = MSG_process_get_data(MSG_process_self());

  int job_idx = task_data->job_idx;

  int *res_idxs = (int *)(task_data->data);

  XBT_INFO("head node -> res id %d", res_idxs[0]);
  
  
  job_exec(job_idx, res_idxs, nodes);

  free(res_idxs);
 
  send_message("server", JOB_COMPLETED, job_idx, NULL);

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
  int size_m;
  char *tmp;
  char *job_ready_str;
  char *jobid_ready;
  char *job_id_str;
  char *res_str;
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
      size_m = asprintf(&sched_message, "%s|%f:C:%s", sched_message, MSG_get_clock(), job_id_str);
      XBT_INFO("sched_message: %s", sched_message);

      //TODO add job_id + msg to send
      break;
    case JOB_SUBMITTED:
      nb_submitted_jobs++;
      job_id_str = jobs[task_data->job_idx].id_str;
      XBT_INFO("Job id %s SUBMITTED, %d jobs submitted", job_id_str, nb_submitted_jobs);
      size_m = asprintf(&sched_message, "%s|%f:S:%s", sched_message, MSG_get_clock(), job_id_str);
      XBT_INFO("sched_message: %s", sched_message);

      break;
    case SCHED_READY:
        
      XBT_DEBUG("Pointer %p", (char **)xbt_dynar_get_ptr( *( (xbt_dynar_t *)task_data->data), 1 ));

      tmp = *(char **)xbt_dynar_get_ptr( *( (xbt_dynar_t *)task_data->data), 1 );

      XBT_INFO("type of receive message from scheduler: %c", *tmp);
      switch (*tmp) {
      case 'J':
	tmp = *(char **)xbt_dynar_get_ptr( *( (xbt_dynar_t *)task_data->data), 2 );
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

	    int nb_res = xbt_dynar_length(res_dynar);
	    int *res_idxs = (int *)malloc(nb_res * sizeof(int)); 
	    xbt_dynar_foreach(res_dynar, i, res_str){
	      res_idxs[i] = atoi(res_str);
	    }
	 
	    send_message(MSG_host_get_name(nodes[head_node]), LAUNCH_JOB, *job_idx, res_idxs);

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
      //size_m = asprintf(&sched_message, "%s0:%f:T", sched_message, MSG_get_clock());
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

int main(int argc, char *argv[])
{
  msg_error_t res = MSG_OK;
  int i;

  json_t *json_workload_profile;

  //Comment to remove debug message
  xbt_log_control_set("batsim.threshold:debug");

  if (argc < 2) { 
    printf("Batsim: Batch System Simulator.\n");
    printf("\n");
    printf("Usage: %s platform_file workload_file\n", argv[0]);
    printf("example: %s platforms/small_platform.xml workload_profiles/test_workload_profile.json\n", argv[0]);
    exit(1);
  }

  json_workload_profile = load_json_workload_profile(argv[2]);
  retrieve_jobs(json_workload_profile);
  retrieve_profiles(json_workload_profile);

  open_uds();

  MSG_init(&argc, argv);


  res = deploy_all(argv[1]);

  if (res == MSG_OK)
    return 0;
  else
    return 1;
}

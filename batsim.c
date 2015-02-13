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

#include "msg/msg.h"            /* Yeah! If you want to use msg, you need to include msg/msg.h */
#include "xbt/sysdep.h"         /* calloc, printf */

/* Create a log channel to have nice outputs. */
#include "xbt/log.h"
#include "xbt/asserts.h"
XBT_LOG_NEW_DEFAULT_CATEGORY(batsim,
                             "Messages specific for this msg example");

#define BAT_SOCK_NAME "/tmp/bat_socket"

#define COMM_SIZE 0
#define COMP_SIZE 0

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
  int job_id;
  xbt_dynar_t *answer_dynar_ptr; 
  const char* src;           // used for logging
} s_task_data_t, *task_data_t;

typedef struct s_job {
  int job_id;
  int profile_id;
  double submission_time;
  double runtime;
} s_job_t, *job_t;


//NOt USE - TOREMOVE
typedef struct s_answer_sched {
  double time;
  int type;
  int nb_jobs_2l;
  int *job_ids_2l;
  int **resources_by_jid_2l;
} s_answer_sched_t;

int nb_jobs = 0;
s_job_t *jobs;

int uds_fd = -1; 

// process functions
static int node(int argc, char *argv[]);
static int server(int argc, char *argv[]);
static int launch_job(int argc, char *argv[]);

/*                                                  */
/* Communication functions  with Unix Domain Socket */
/*                                                  */
int send_uds(char *msg) 
{ 
  int32_t lg = strlen(msg);
  XBT_INFO("send_uds, lg: %d", lg);
  write(uds_fd, &lg, 4);
  write(uds_fd, msg, lg);
  free(msg);
}

char *recv_uds()
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

void open_uds() 
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

static void task_free(struct msg_task ** task)
{
  if(*task != NULL){
    xbt_free(MSG_task_get_data(*task));
    MSG_task_destroy(*task);
    *task = NULL;
  }
}

void msg_send(const char *dst, e_task_type_t type, int job_id, xbt_dynar_t *answer_dynar_ptr)
{
  msg_task_t task_sent;
  task_data_t req_data = xbt_new0(s_task_data_t,1);
  req_data->type = type;
  req_data->job_id = job_id;
  req_data->answer_dynar_ptr = answer_dynar_ptr;
  req_data->src =  MSG_host_get_name(MSG_host_self());
  task_sent = MSG_task_create(NULL, COMP_SIZE, COMM_SIZE, req_data);

  MSG_task_send(task_sent, dst);
}

static int send_sched(int argc, char *argv[])
{
  double t_send = MSG_get_clock();
  double t_answer = 0;
  
  char *message_recv = NULL;
  char *message_send =  MSG_process_get_data(MSG_process_self());

  xbt_dynar_t answer_dynar;

  send_uds(message_send);
  message_recv = recv_uds();
  
  answer_dynar = xbt_str_split(message_recv, ":");	

  t_answer = atof( *(char **)xbt_dynar_get_ptr(answer_dynar, 1) ); 

  //waiting before consider the sched's answer
  MSG_process_sleep(t_answer - t_send);

  //signal 
  msg_send("server", SCHED_READY, 0, &answer_dynar);

  free(message_recv);
  return 0;
}

static int launch_job(int argc, char *argv[])
{
  xbt_dynar_t slaves_dynar;
  int slaves_count = 0;
  msg_host_t *slaves = NULL;
  double task_comp_size = 1000000;
  double task_comm_size = 10000;
  double *computation_amount = NULL;
  double *communication_amount = NULL;
  msg_task_t ptask = NULL;
  int i, j;
 
  XBT_INFO("Launch_job_poy1");
  int *job_id = MSG_process_get_data(MSG_process_self());
  XBT_INFO("Launch_job_poy2");
  slaves_dynar = MSG_hosts_as_dynar();
  slaves_count = xbt_dynar_length(slaves_dynar);
  slaves = xbt_dynar_to_array(slaves_dynar);
  
  computation_amount = xbt_new0(double, slaves_count);
  communication_amount = xbt_new0(double, slaves_count * slaves_count);
  
  task_comp_size = task_comp_size + rand() % 5000000;

  for (i = 0; i < slaves_count; i++)
    computation_amount[i] = task_comp_size;

  for (i = 0; i < slaves_count; i++)
    for (j = i + 1; j < slaves_count; j++)
      communication_amount[i * slaves_count + j] = task_comm_size;

  XBT_INFO("Launch Job id %d", *job_id);
  //slaves_count = 2;
  //MSG_process_sleep(rand() % 1000);
  ptask = MSG_parallel_task_create("parallel task",
                                   slaves_count, slaves,
                                   computation_amount,
                                   communication_amount, NULL);
  MSG_parallel_task_execute(ptask);

  MSG_task_destroy(ptask);

  msg_send("server", JOB_COMPLETED, *job_id, NULL);

  /* There is no need to free that! */
  /*   free(communication_amount); */
  /*   free(computation_amount); */

  //free(slaves);
  return 0;
}

static int node(int argc, char *argv[]) 
{
  const char *node_id = MSG_host_get_name(MSG_host_self());

  msg_task_t task_received = NULL;
  task_data_t task_data;
  int flag = 1;
  int type = -1;
  int job_id = -1; //TODO change this

  XBT_INFO("I am %s", node_id);
  
  while(1) {
    MSG_task_receive(&(task_received), node_id);
    
    task_data = (task_data_t) MSG_task_get_data(task_received);
    type = task_data->type;

    XBT_INFO("Task received %s, type %s", node_id, task_type2str[type]);
    
    if (type == FINALIZE) break;

    switch (type) {
    case LAUNCH_JOB:
     
      XBT_INFO("Launch_job");
      
      job_id = task_data->job_id; //TODO need malloc ??
      XBT_INFO("Launch_job_yop %d", job_id );
      MSG_process_create("launch_job", launch_job, &job_id, MSG_host_self());
      
      break;
    }
    task_free(&task_received);
  }

}

static int jobs_submitter(int argc, char *argv[]) {

  double submission_time = 0.0;
  int job2submit_idx = 0;
  xbt_dynar_t jobs2sub_dynar;
  double t = MSG_get_clock();
  int job_id, i;

  while (job2submit_idx < nb_jobs) {
    submission_time = jobs[job2submit_idx].submission_time;

    jobs2sub_dynar = xbt_dynar_new(sizeof(int), NULL);
    while(submission_time == jobs[job2submit_idx].submission_time) {
      xbt_dynar_push_as(jobs2sub_dynar, int, jobs[job2submit_idx].job_id);
      XBT_INFO("job2submit_idx %d, job_id %d", job2submit_idx, jobs[job2submit_idx].job_id);
      job2submit_idx++;
      if (job2submit_idx == nb_jobs) break;
    }
    
    MSG_process_sleep(submission_time - t);
    t = MSG_get_clock();

    xbt_dynar_foreach(jobs2sub_dynar, i, job_id) { 
      msg_send("server", JOB_SUBMITTED, job_id, NULL);
    }
    xbt_dynar_free(&jobs2sub_dynar);
  }
  return 0;
}

int server(int argc, char *argv[]) {

  xbt_dynar_t nodes_dynar;
  msg_host_t *nodes;
  msg_host_t node;
  int nb_nodes = 0;
    
  msg_task_t task_received = NULL;
  task_data_t task_data;

  int nb_completed_jobs = 0;
  int nb_submitted_jobs = 0;
  int sched_ready = true;
  char *sched_message = "";
  char *tmp;
  char *jobid_ready;
  int size_m = 0;
  double t = 0;
  int i;

  nodes_dynar = MSG_hosts_as_dynar();
  xbt_dynar_remove_at(nodes_dynar, 0, NULL);
  nb_nodes = xbt_dynar_length(nodes_dynar);
  nodes = xbt_dynar_to_array(nodes_dynar);

  while ( (nb_completed_jobs < nb_jobs) || !sched_ready ) {

    // wait message node, submitter, scheduler...
    MSG_task_receive(&(task_received), "server");

    task_data = (task_data_t) MSG_task_get_data(task_received);

    XBT_INFO("Server receive Task/message type %s:", task_type2str[task_data->type]);

    switch (task_data->type) {
    case JOB_COMPLETED:
      nb_completed_jobs++;
      XBT_INFO("Job id %d COMPLETED, %d jobs completed", task_data->job_id, nb_completed_jobs);
      size_m = asprintf(&sched_message, "%s0:%f:C:%d|", sched_message, MSG_get_clock(), task_data->job_id);
      XBT_INFO("sched_message: %s", sched_message);

      //TODO add job_id + msg to send
      break;
    case JOB_SUBMITTED:
      nb_submitted_jobs++;
      XBT_INFO("Job id %d SUBMITTED, %d jobs submitted", task_data->job_id, nb_submitted_jobs);
      size_m = asprintf(&sched_message, "%s0:%f:S:%d|", sched_message, MSG_get_clock(), task_data->job_id);
      XBT_INFO("sched_message: %s", sched_message);

      break;
    case SCHED_READY:
      //process scheduler message
      //up to now only jobs to launch
      // 0:timestamp:J:jid1=0,1,3;jid2=5,7,8,9
      // 0:        1:2:3
      
      tmp = *(char **)xbt_dynar_get_ptr( *(task_data->answer_dynar_ptr), 2 );

      XBT_INFO("type of receive message from scheduler: %s", tmp);
      switch (*tmp) {
      case 'J':
	tmp = *(char **)xbt_dynar_get_ptr( *(task_data->answer_dynar_ptr), 3 );
	XBT_INFO("poy");
	xbt_dynar_t jobs_ready_dynar = xbt_str_split(tmp, ";");
	
	//xbt_dynar_shift(jobs_ready_dynar, &jobid_ready); 
	//XBT_INFO("job_ready: %s", jobid_ready);
	//TODO COMPLETE LAUNCH JOB
	//msg_send(MSG_host_get_name(nodes[0]), LAUNCH_JOB, atoi(jobid_ready), NULL);

	xbt_dynar_foreach(jobs_ready_dynar, i, jobid_ready) {
	  if (jobid_ready != NULL) {
	    XBT_INFO("job_ready: %s", jobid_ready);
	    //TODO COMPLETE LAUNCH JOB
	    msg_send(MSG_host_get_name(nodes[i % nb_nodes]), LAUNCH_JOB, atoi(jobid_ready), NULL);
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
      
      // core dump why ???
      //xbt_dynar_free(task_data->answer_dynar_ptr);

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
    msg_send(MSG_host_get_name(nodes[i]), FINALIZE, 0, NULL);
  }

}


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
  first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);
 	
  xbt_dynar_foreach(all_hosts, i, host) {
    if (i!=0) {
      XBT_INFO("Create node process! %d", i);
      MSG_process_create("node", node, NULL, host);
    }
  }

  MSG_process_create("server", server, NULL, first_host);
  MSG_process_create("jobs_submitter", jobs_submitter, NULL, first_host);
    
  res = MSG_main();
  xbt_dynar_free(&all_hosts);

  XBT_INFO("Simulation time %g", MSG_get_clock());
  return res;
}


int main(int argc, char *argv[])
{
  msg_error_t res = MSG_OK;
  int i;

  //Comment to remove debug message
  xbt_log_control_set("batsim.threshold:debug");

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

  /* generate fake job workloak */
  nb_jobs = 3;
  jobs = malloc(nb_jobs * sizeof(s_job_t));

  for(i=0; i<nb_jobs; i++) {
    jobs[i].job_id = i;
    jobs[i].profile_id = i;
    jobs[i].submission_time = 10.0 * (i+1);
    jobs[i].runtime = -1;
  }

  /*                       */
  /* TODO load job profile */
  /*                       */

  res = deploy_all(argv[1]);

  if (res == MSG_OK)
    return 0;
  else
    return 1;
}

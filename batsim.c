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
XBT_LOG_NEW_DEFAULT_CATEGORY(msg_test,
                             "Messages specific for this msg example");



#define BAT_SOCK_NAME "/tmp/bat_socket"

#define COMM_SIZE 0
#define COMP_SIZE 0

/**
 * Types of tasks exchanged between nodes.
 */
typedef enum {
  ECHO,
  FINALIZE,
  LAUNCH_JOB,
  JOB_COMPLETED,
  KILL_JOB,
  SUSPEND_JOB,
} e_task_type_t;

char *task_type2str[] = {
  "ECHO",
  "FINALIZE",
  "LAUNCH_JOB",
  "JOB_COMPLETED",
  "KILL_JOB",
  "SUSPEND_JOB"
};

/*
 * Data attached with the tasks sent and received
 */
typedef struct s_task_data {
  e_task_type_t type;                     // type of task
  int job_id;
  const char* src;           // used for logging
} s_task_data_t, *task_data_t;

typedef struct s_job {
  int job_id;
  int profile_id;
  double submission_time;
  double runtime;
} s_job_t, *job_t;

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
  write(uds_fd, &lg, 4);
  write(uds_fd, msg, lg);
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

void msg_send(const char *dst, e_task_type_t type, int job_id)
{
  msg_task_t task_sent;
  task_data_t req_data = xbt_new0(s_task_data_t,1);
  req_data->type = type;
  req_data->job_id = job_id;
  req_data->src =  MSG_host_get_name(MSG_host_self());
  task_sent = MSG_task_create(NULL, COMP_SIZE, COMM_SIZE, req_data);

  MSG_task_send(task_sent, dst);
}

void free_s_answer_sched( s_answer_sched_t *ans) {
  //TODO ?
}

void send_sched_job_completed(int job_id) {
  char buffer[256];
  snprintf(buffer, 256, "0:%d:C:%d", MSG_get_clock(), job_id);
  send_uds(buffer);
}

void recv_sched_job_recv() {
  s_answer_sched_t answer;
  char *str_answer = recv_uds();
  //split string
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

  int *job_id = MSG_process_get_data(MSG_process_self() );

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

  msg_send("server", JOB_COMPLETED, *job_id);

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
      //XBT_INFO("Launch_job");
      job_id = task_data->job_id; //TODO need malloc ??
      MSG_process_create("launch_job", launch_job, &job_id, MSG_host_self());
      break;
    }
    task_free(&task_received);
    task_received = NULL;

  }

}

int server(int argc, char *argv[]) {
  xbt_dynar_t nodes_dynar;
  msg_host_t *nodes;
  msg_host_t node;
  int nb_nodes = 0;
  msg_task_t task_sent;
  msg_task_t task_received = NULL;
  task_data_t task_data;
  int type = -1;
  msg_error_t ret;
  double timeout = -1.0;
  double submission_time = 0.0;
  int nb_completed_jobs = 0;
  int job2submit_idx = 0;
  int jobs2submit = FALSE;
  xbt_dynar_t jobs2sub_dynar;
  int job_id;
  int i;
  
  const char *server_id = MSG_host_get_name(MSG_host_self());

  nodes_dynar = MSG_hosts_as_dynar();
  xbt_dynar_remove_at(nodes_dynar, 0, NULL);
  nb_nodes = xbt_dynar_length(nodes_dynar);
  nodes = xbt_dynar_to_array(nodes_dynar);
  
  do {
    //
    if (job2submit_idx < nb_jobs) { //remains job to launch
      submission_time = jobs[job2submit_idx].submission_time;
      jobs2submit = TRUE;
      jobs2sub_dynar = xbt_dynar_new(sizeof(int), NULL);
      while(submission_time == jobs[job2submit_idx].submission_time) {
	xbt_dynar_push_as(jobs2sub_dynar, int, jobs[job2submit_idx].job_id);
	XBT_INFO("job2submit_idx %d, job_id %d", job2submit_idx, jobs[job2submit_idx].job_id);
	job2submit_idx++;
      }
      //job to launch
    } else {
      XBT_INFO("No more jobs to submit");
      timeout = -1;
    }
    
    /* server is waiting for communication from nodes */
    do { 
      if (jobs2submit) {
	timeout = submission_time - MSG_get_clock();
      } else {
	timeout = -1;
      }
     
      ret = MSG_task_receive_with_timeout(&(task_received), "server", timeout);
      if (ret ==  MSG_OK) {
	task_data = (task_data_t) MSG_task_get_data(task_received);
	type = task_data->type;
	//test if a job is completed
	if (type == JOB_COMPLETED) {
	  nb_completed_jobs++;
	  XBT_INFO("Job id %d COMPLETED, %d jobs completed", task_data->job_id, nb_completed_jobs);
	  //XBT_INFO("TODO 1: INFORM SCHED, TODO 2: wait some fraction of sec if more jobs completed");
	  // TODO test when several jobs complete at same time
	  send_sched_job_completed(task_data->job_id);
	  recv_sched()
	  

	} else {
	  
	  XBT_INFO("TODO: server receive message with type: %s", task_type2str[type]);
	  
	}

	task_free(&task_received);

      }

      //XBT_INFO("**** jobs2submit %d next_job2submit_idx %d, ret %d", jobs2submit, job2submit_idx, ret); 

      if ( (ret == MSG_TIMEOUT) || (MSG_get_clock() >= submission_time) ) {
	if (jobs2submit) {
	//
	  XBT_INFO("TODO : INFORM SCHED that some jobs are submitted now and retrieve jobs to launched w/ resource assignement");
	
	  xbt_dynar_foreach(jobs2sub_dynar, i, job_id) {
	    XBT_INFO("Job to launch: %d", job_id); 
	    msg_send(MSG_host_get_name(nodes[i % nb_nodes]), LAUNCH_JOB, job_id);
	  }

	  jobs2submit = FALSE;
	  xbt_dynar_free(&jobs2sub_dynar); 

	}
      }

	
    } while (jobs2submit);
    

  } while ( nb_completed_jobs < nb_jobs );

  XBT_INFO("All jobs completed, time to finalize");
       
  for(i=0; i < nb_nodes; i++) {
      msg_send(MSG_host_get_name(nodes[i]), FINALIZE, 0);
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
 	
  MSG_process_create("server", server, NULL, first_host);

  xbt_dynar_foreach(all_hosts, i, host) {
    if (i!=0) {
      XBT_INFO("Create node process! %d", i);
      MSG_process_create("node", node, NULL, host);
    }
  }
    
  res = MSG_main();
  xbt_dynar_free(&all_hosts);

  XBT_INFO("Simulation time %g", MSG_get_clock());
  return res;
}


int main(int argc, char *argv[])
{
  msg_error_t res = MSG_OK;
  int i;

  //srand(time(NULL));

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

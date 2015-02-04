/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <stdio.h>
#include "msg/msg.h"            /* Yeah! If you want to use msg, you need to include msg/msg.h */
#include "xbt/sysdep.h"         /* calloc, printf */

/* Create a log channel to have nice outputs. */
#include "xbt/log.h"
#include "xbt/asserts.h"
XBT_LOG_NEW_DEFAULT_CATEGORY(msg_test,
                             "Messages specific for this msg example");

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
  int profile_id;
  const char* src;           // used for logging
} s_task_data_t, *task_data_t;

typedef struct s_job_info {
  int job_id;
  int profile_id;
  double submission_time;
} s_jobd_info_t, *job_info_t;

int nb_jobs = 0;
s_jobd_info_t *job_infos;

// process functions
static int node(int argc, char *argv[]);
static int server(int argc, char *argv[]);
static int launch_job(int argc, char *argv[]);

static void task_free(struct msg_task ** task)
{
  if(*task != NULL){
    xbt_free(MSG_task_get_data(*task));
    MSG_task_destroy(*task);
    *task = NULL;
  }
}

void msg_send(const char *dst, e_task_type_t type, int job_id, int profile_id)
{
  msg_task_t task_sent;
  task_data_t req_data = xbt_new0(s_task_data_t,1);
  req_data->type = type;
  req_data->src =  MSG_host_get_name(MSG_host_self());
  task_sent = MSG_task_create(NULL, COMP_SIZE, COMM_SIZE, req_data);

  MSG_task_send(task_sent, dst);
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

  int *parallel_job_ref = MSG_process_get_data(MSG_process_self() );

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

  XBT_INFO("Launch Job, parallel ref %d", *parallel_job_ref);
  //slaves_count = 2;
  //MSG_process_sleep(rand() % 1000);
  ptask = MSG_parallel_task_create("parallel task",
                                   slaves_count, slaves,
                                   computation_amount,
                                   communication_amount, NULL);
  MSG_parallel_task_execute(ptask);

  MSG_task_destroy(ptask);

  msg_send("server", JOB_COMPLETED, 0, 0);

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
  int parallel_ref = 10; //TODO change this

  XBT_INFO("I am %s", node_id);
  
  while(1) {
    MSG_task_receive(&(task_received), node_id);
    
    task_data = (task_data_t) MSG_task_get_data(task_received);
    type = task_data->type;
    
    XBT_INFO("Task received %s, type %s", node_id, task_type2str[type]);
    
    if (type == FINALIZE) break;

    switch (type) {
    case LAUNCH_JOB:
	MSG_process_create("launch_job", launch_job, &parallel_ref, MSG_host_self());
	break;
    }
    task_free(&task_received);
    task_received = NULL;

  }

}

int server(int argc, char *argv[]) {
  xbt_dynar_t nodes;
  msg_host_t node;
  int nb_nodes = 0;
  msg_task_t task_sent;
  msg_task_t task_received = NULL;
  msg_comm_t comm_receive = NULL; 
  int i;


  const char *server_id = MSG_host_get_name(MSG_host_self());

  nodes = MSG_hosts_as_dynar();
  xbt_dynar_remove_at(nodes, 0, NULL); 		
  nb_nodes = xbt_dynar_length(nodes);

  
  xbt_dynar_foreach(nodes, i, node) {
    msg_send(MSG_host_get_name(node), LAUNCH_JOB, 0, 0);
  }
  
  for(i=0; i < nb_nodes; i++) {

     comm_receive = MSG_task_irecv(&(task_received), "server");
     //MSG_comm_wait(comm_receive, -1);
     while (!MSG_comm_test(comm_receive)) {
       XBT_INFO("Server is waiting comm");
       MSG_process_sleep(0.05);
     } 
     task_free(&task_received);
     MSG_comm_destroy(comm_receive);
     comm_receive = NULL;
  }

  /* send finalize */ 
  xbt_dynar_foreach(nodes, i, node) {
    msg_send(MSG_host_get_name(node), FINALIZE, 0, 0);
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
  nb_jobs = 10;
  job_infos = malloc(nb_jobs * sizeof(s_jobd_info_t));

  for(i=0; i<nb_jobs; i++) {
    job_infos[i].job_id = i+1;
    job_infos[i].profile_id = 2*i;
    job_infos[i].submission_time = 10.0 * (i+1);
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

/* Copyright (c) 2007-2014. The SimGrid Team.
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

/** @addtogroup MSG_examples
 * 
 * - <b>parallel_task/parallel_task.c</b>: Demonstrates the use of
 *   @ref MSG_parallel_task_create, to create special tasks that run
 *   on several hosts at the same time. The resulting simulations are
 *   very close to what can be achieved in @ref SD_API, but still
 *   allows to use the other features of MSG (it'd be cool to be able
 *   to mix interfaces, but it's not possible ATM).
 */

int test(int argc, char *argv[]);
msg_error_t test_all(const char *platform_file);

/** Emitter function  */
int test(int argc, char *argv[])
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

  
  //slaves_count = 2;
  //MSG_process_sleep(rand() % 1000);
  ptask = MSG_parallel_task_create("parallel task",
                                   slaves_count, slaves,
                                   computation_amount,
                                   communication_amount, NULL);
  MSG_parallel_task_execute(ptask);

  MSG_task_destroy(ptask);
  /* There is no need to free that! */
/*   free(communication_amount); */
/*   free(computation_amount); */


  XBT_INFO("Goodbye now! %f", MSG_get_clock());
  //free(slaves);
  return 0;
}

int server(int argc, char *argv[]) {
  xbt_dynar_t all_hosts;
  msg_host_t first_host;
  int i;

  all_hosts = MSG_hosts_as_dynar();
  first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);

  for(i=0; i<10; i++) {
  //  for(i=0; i<10; i++) {
    MSG_process_create("test", test, NULL, first_host);
    //MSG_process_sleep(100000);
  }
}

/** Test function */
msg_error_t test_all(const char *platform_file)
{
  msg_error_t res = MSG_OK;
  xbt_dynar_t all_hosts;
  msg_host_t first_host;

  int i;

  MSG_config("workstation/model", "ptask_L07");
  MSG_create_environment(platform_file);


  all_hosts = MSG_hosts_as_dynar();
  first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);

  MSG_process_create("server", server, NULL, first_host);

  /*
  all_hosts = MSG_hosts_as_dynar();
  first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);

  for(i=0; i<10; i++) {
  //  for(i=0; i<10; i++) {
    MSG_process_create("test", test, NULL, first_host);
    //MSG_process_sleep(100000);
  }
  */
  res = MSG_main();
  xbt_dynar_free(&all_hosts);

  XBT_INFO("Simulation time %g", MSG_get_clock());
  return res;
}

int main(int argc, char *argv[])
{
  msg_error_t res = MSG_OK;

  srand(time(NULL));

  MSG_init(&argc, argv);
  if (argc < 2) {
    printf("Usage: %s platform_file\n", argv[0]);
    printf("example: %s msg_platform.xml\n", argv[0]);
    exit(1);
  }
  res = test_all(argv[1]);

  if (res == MSG_OK)
    return 0;
  else
    return 1;
}

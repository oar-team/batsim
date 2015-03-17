/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */

#include <msg/msg.h>

#include <xbt/sysdep.h>         /* calloc, printf */
/* Create a log channel to have nice outputs. */
#include <xbt/log.h>
#include <xbt/asserts.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(batexec, "Batexec");

#include "job.h"
#include "utils.h"

int nb_nodes = 0;
msg_host_t *nodes;


/**
 * \brief Execute jobs alone 
 *
 * Execute jobs sequentially without server and scheduler
 *
 */
static int job_launcher(int argc, char *argv[])
{
  int job_idx; 
  s_job_t job;
  int i;
  double t;
  int *res_idxs;

  for(job_idx = 0; job_idx < nb_jobs; job_idx++) {
    t = MSG_get_clock();
    job = jobs[job_idx];

    res_idxs = (int *) malloc( (job.nb_res) * sizeof(int));

    for(i = 0; i < job.nb_res ; i++) {
      res_idxs[i] = i;
    }  

    job_exec(job_idx, job.nb_res, res_idxs, nodes, NULL);
    XBT_INFO("Job id %d, job simulation time: %f", job.id,  MSG_get_clock() - t);
    free(res_idxs);
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
  xbt_dynar_get_cpy(all_hosts, 0, &first_host);
  //first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);
  xbt_dynar_remove_at(all_hosts, 0, NULL);
  
  nb_nodes = xbt_dynar_length(all_hosts);
  nodes = xbt_dynar_to_array(all_hosts);
  //xbt_dynar_free(&all_hosts);

  XBT_INFO("Nb nodes: %d", nb_nodes);

  MSG_process_create("job_launcher", job_launcher, NULL, first_host);
    
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
  xbt_log_control_set("batexec.threshold:debug");

  if (argc < 2) {
    printf("Batexec: execute a list of jobs in FIFO.\n");
    printf("Resources are assigned from 0, only one job is running at a time\n");    
    printf("\n");
    printf("Usage: %s platform_file workload_file\n", argv[0]);
    printf("example: %s ../platforms/small_platform.xml ../workload_profiles/test_workload_profile.json\n", argv[0]);
    exit(1);
  }

  json_workload_profile = load_json_workload_profile(argv[2]);
  retrieve_jobs(json_workload_profile);
  retrieve_profiles(json_workload_profile);

  MSG_init(&argc, argv);


  res = deploy_all(argv[1]);
    
 if (res == MSG_OK)
    return 0;
  else
    return 1;

}

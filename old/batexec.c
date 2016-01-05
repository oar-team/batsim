/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */

#include <simgrid/msg.h>

#include <xbt/sysdep.h>         /* calloc, printf */
/* Create a log channel to have nice outputs. */
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <float.h>
#include <math.h>
#include <stdbool.h>

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
    (void) argc;
    (void) argv;

    unsigned int job_index;
    s_job_t * job;

    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {
        int t = MSG_get_clock();
        int * res_idxs = xbt_new(int, job->nb_res);

        for (int i = 0; i < job->nb_res; i++)
            res_idxs[i] = i;

        job_exec(job->id, job->nb_res, res_idxs, nodes, DBL_MAX);
        XBT_INFO("Job id %d, job simulation time: %f", job->id,  MSG_get_clock() - t);
        free(res_idxs);
    }

    return 0;
}

msg_error_t deploy_all(const char *platform_file, bool smpi_used)
{
    msg_error_t res = MSG_OK;
    xbt_dynar_t all_hosts;
    msg_host_t first_host;

    if (!smpi_used)
      MSG_config("host/model", "ptask_L07");

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

    //Comment to remove debug message
    xbt_log_control_set("batexec.threshold:debug");

    if (argc < 2)
    {
        printf("Batexec: execute a list of jobs in FIFO.\n");
        printf("Resources are assigned from 0, only one job is running at a time\n");
        printf("\n");
        printf("Usage: %s platform_file workload_file\n", argv[0]);
        printf("example: %s ../platforms/small_platform.xml ../workload_profiles/test_workload_profile.json\n", argv[0]);
        exit(1);
    }

    json_t *json_workload_profile;

    json_workload_profile = load_json_workload_profile(argv[2]);
    retrieve_jobs(json_workload_profile);
    retrieve_profiles(json_workload_profile);
    checkJobsAndProfilesValidity();

    MSG_init(&argc, argv);

    // Register all smpi jobs app and init SMPI
    bool smpi_used = register_smpi_app_instances();

    msg_error_t res = deploy_all(argv[1], smpi_used);

    json_decref(json_workload_profile);
    // Let's clear global allocated data
    freeJobStructures();
    free(nodes);

    if (res == MSG_OK)
        return 0;
    else
        return 1;

}

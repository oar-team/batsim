/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(job, "job");

void profile_exec(const char *profile_str, int job_id, int nb_res, msg_host_t *job_res, xbt_dict_t * allocatedStuff)
{
    double *computation_amount;
    double *communication_amount;
    msg_task_t ptask;

    profile_t profile = xbt_dict_get(profiles, profile_str);

    if (strcmp(profile->type, "msg_par") == 0)
    {
        // These amounts are deallocated by SG
        computation_amount = malloc(nb_res * sizeof(double));
        communication_amount = malloc(nb_res * nb_res * sizeof(double));

        double *cpu = ((s_msg_par_t *)(profile->data))->cpu;
        double *com = ((s_msg_par_t *)(profile->data))->com;

        memcpy(computation_amount , cpu, nb_res * sizeof(double));
        memcpy(communication_amount, com, nb_res * nb_res * sizeof(double));

        char * tname = NULL;
        asprintf(&tname, "p %d", job_id);
        XBT_INFO("Creating task '%s'", tname);

        //ptask = malloc(sizeof(msg_task_t *));
        ptask = MSG_parallel_task_create(tname,
                                         nb_res, job_res,
                                         computation_amount,
                                         communication_amount, NULL);
        free(tname);
        xbt_dict_set(*allocatedStuff, "task", (void*)ptask, freeTask);
        msg_error_t err = MSG_parallel_task_execute(ptask);
        xbt_assert(err == MSG_OK);
    }
    else if (strcmp(profile->type, "msg_par_hg") == 0)
    {
        double cpu = ((s_msg_par_hg_t *)(profile->data))->cpu;
        double com = ((s_msg_par_hg_t *)(profile->data))->com;

        // These amounts are deallocated by SG
        computation_amount = malloc(nb_res * sizeof(double));
        communication_amount = malloc(nb_res * nb_res * sizeof(double));

        XBT_DEBUG("msg_par_hg: nb_res: %d , cpu: %f , com: %f", nb_res, cpu, com);

        for (int i = 0; i < nb_res; i++)
            computation_amount[i] = cpu;

        for (int j = 0; j < nb_res; j++)
            for (int i = 0; i < nb_res; i++)
                communication_amount[nb_res * j + i] = com;

        char * tname = NULL;
        asprintf(&tname, "hg %d", job_id);
        XBT_INFO("Creating task '%s'", tname);

        //ptask = malloc(sizeof(msg_task_t *));
        ptask = MSG_parallel_task_create(tname,
                                         nb_res, job_res,
                                         computation_amount,
                                         communication_amount, NULL);
        free(tname);
        xbt_dict_set(*allocatedStuff, "task", (void*)ptask, freeTask);
        msg_error_t err = MSG_parallel_task_execute(ptask);
        xbt_assert(err == MSG_OK);
    }
    else if (strcmp(profile->type, "composed") == 0)
    {
        s_composed_prof_t * data = (s_composed_prof_t *) profile->data;
        int nb = data->nb;
        int lg_seq = data->lg_seq;
        char **seq = data->seq;

        XBT_DEBUG("composed: nb: %d, lg_seq: %d", nb, lg_seq);

        for (int i = 0; i < nb; i++)
        {
            for (int j = 0; j < lg_seq; j++)
            {
                profile_exec(seq[j], job_id, nb_res, job_res, allocatedStuff);
            }
        }
    }
    else if (strcmp(profile->type, "delay") == 0)
    {
        double delay = ((s_delay_t *)(profile->data))->delay;
        MSG_process_sleep(delay);

    }
    else if (strcmp(profile->type, "smpi") == 0)
    {
        xbt_die("Profile with type %s is not yet implemented", profile->type);
    }
    else
    {
        xbt_die("Profile with type %s is not supported", profile->type);
    }
}

/**
 * \brief Load workload with jobs' profiles file
 */

void job_exec(int job_id, int nb_res, int *res_idxs, msg_host_t *nodes, xbt_dict_t * allocatedStuff)
{
    int dictCreatedHere = 0;

    if (allocatedStuff == NULL)
    {
        allocatedStuff = malloc(sizeof(xbt_dict_t));
        *allocatedStuff = xbt_dict_new();
        dictCreatedHere = 1;
    }

    s_job_t * job = jobFromJobID(job_id);
    XBT_INFO("job_exec: jobID %d, job=%p", job_id, job);

    msg_host_t * job_res = (msg_host_t *) malloc(nb_res * sizeof(s_msg_host_t));
    xbt_dict_set(*allocatedStuff, "hosts", job_res, free);

    for(int i = 0; i < nb_res; i++)
        job_res[i] = nodes[res_idxs[i]];

    profile_exec(job->profile, job_id, nb_res, job_res, allocatedStuff);

    if (dictCreatedHere)
    {
        xbt_dict_free(allocatedStuff);
        free(allocatedStuff);
    }
}

void freeTask(void * task)
{
    msg_task_t t = (msg_task_t) task;

    const static int doLeak = 0;

    if (doLeak)
    {
        XBT_INFO("freeing task '%s' (with memory leak)", MSG_task_get_name(t));
        XBT_INFO("freeing task (with memory leak) done");
    }
    else
    {
        // todo: make this work -> SG mailing list? cancelling the task might be better BEFORE killing the associated process...
        XBT_INFO("freeing task '%s'", MSG_task_get_name(t));
        //MSG_task_cancel(t);
        MSG_task_destroy(t);
        XBT_INFO("freeing task done");
    }
}
/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(task, "task");

//! The input data of a killerDelay
typedef struct s_killer_delay_data
{
    msg_task_t task; //! The task that will be cancelled if the walltime is reached
    double walltime; //! The number of seconds to wait before cancelling the task
} KillerDelayData;

int killerDelay(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    KillerDelayData * data = MSG_process_get_data(MSG_process_self());

    // The sleep can either stop normally (res=MSG_OK) or be cancelled when the task execution completed (res=MSG_TASK_CANCELED)
    msg_error_t res = MSG_process_sleep(data->walltime);

    if (res == MSG_OK)
    {
        // If we had time to sleep until walltime (res=MSG_OK), the task execution is not over and must be cancelled
        XBT_INFO("Cancelling task '%s'", MSG_task_get_name(data->task));
        MSG_task_cancel(data->task);
    }

    free(data);

    return 0;
}

/**
 * @brief Executes the profile of a given job
 * @param[in] profile_str The name of the profile to execute
 * @param[in] job_id The job number
 * @param[in] nb_res The number of resources on which the job will be executed
 * @param[in] job_res The resources on which the job will be executed
 * @param[in,out] remainingTime The time remaining to execute the full profile
 * @return 1 if the profile had been executed in time, 0 if the walltime had been reached
 */
int profile_exec(const char *profile_str, int job_id, int nb_res, msg_host_t *job_res, double * remainingTime)
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

        ptask = MSG_parallel_task_create(tname,
                                         nb_res, job_res,
                                         computation_amount,
                                         communication_amount, NULL);
        free(tname);

        // Let's spawn a process which will wait until walltime and cancel the task if needed
        KillerDelayData * killData = malloc(sizeof(KillerDelayData));
        killData->task = ptask;
        killData->walltime = *remainingTime;

        msg_process_t killProcess = MSG_process_create("killer", killerDelay, killData, MSG_host_self());

        double timeBeforeExecute = MSG_get_clock();
        XBT_INFO("Executing task '%s'", MSG_task_get_name(ptask));
        msg_error_t err = MSG_parallel_task_execute(ptask);
        *remainingTime = *remainingTime - (MSG_get_clock() - timeBeforeExecute);

        int ret = 1;
        if (err == MSG_OK)
            SIMIX_process_throw(killProcess, cancel_error, 0, "wake up");
        else if (err == MSG_TASK_CANCELED)
            ret = 0;
        else
            xbt_die("A task execution had been stopped by an unhandled way (err = %d)", err);

        XBT_INFO("Task '%s' finished", MSG_task_get_name(ptask));
        MSG_task_destroy(ptask);
        return ret;

    }
    else if (strcmp(profile->type, "msg_par_hg") == 0)
    {
        double cpu = ((s_msg_par_hg_t *)(profile->data))->cpu;
        double com = ((s_msg_par_hg_t *)(profile->data))->com;

        // These amounts are deallocated by SG
        computation_amount = malloc(nb_res * sizeof(double));
        communication_amount = malloc(nb_res * nb_res * sizeof(double));

        //XBT_DEBUG("msg_par_hg: nb_res: %d , cpu: %f , com: %f", nb_res, cpu, com);

        for (int i = 0; i < nb_res; i++)
            computation_amount[i] = cpu;

        for (int j = 0; j < nb_res; j++)
            for (int i = 0; i < nb_res; i++)
                communication_amount[nb_res * j + i] = com;

        char * tname = NULL;
        asprintf(&tname, "hg %d", job_id);
        XBT_INFO("Creating task '%s'", tname);

        ptask = MSG_parallel_task_create(tname,
                                         nb_res, job_res,
                                         computation_amount,
                                         communication_amount, NULL);
        free(tname);

        // Let's spawn a process which will wait until walltime and cancel the task if needed
        KillerDelayData * killData = malloc(sizeof(KillerDelayData));
        killData->task = ptask;
        killData->walltime = *remainingTime;

        msg_process_t killProcess = MSG_process_create("killer", killerDelay, killData, MSG_host_self());

        double timeBeforeExecute = MSG_get_clock();
        XBT_INFO("Executing task '%s'", MSG_task_get_name(ptask));
        msg_error_t err = MSG_parallel_task_execute(ptask);
        *remainingTime = *remainingTime - (MSG_get_clock() - timeBeforeExecute);

        int ret = 1;
        if (err == MSG_OK)
            SIMIX_process_throw(killProcess, cancel_error, 0, "wake up");
        else if (err == MSG_TASK_CANCELED)
            ret = 0;
        else
            xbt_die("A task execution had been stopped by an unhandled way (err = %d)", err);

        XBT_INFO("Task '%s' finished", MSG_task_get_name(ptask));
        MSG_task_destroy(ptask);
        return ret;
    }
    else if (strcmp(profile->type, "composed") == 0)
    {
        s_composed_prof_t * data = (s_composed_prof_t *) profile->data;
        int nb = data->nb;
        int lg_seq = data->lg_seq;
        char **seq = data->seq;

        for (int i = 0; i < nb; i++)
            for (int j = 0; j < lg_seq; j++)
                if (profile_exec(seq[j], job_id, nb_res, job_res, remainingTime) == 0)
                    return 0;
    }
    else if (strcmp(profile->type, "delay") == 0)
    {
        double delay = ((s_delay_t *)(profile->data))->delay;

        if (delay < *remainingTime)
        {
            XBT_INFO("Sleeping the whole task length");
            MSG_process_sleep(delay);
            XBT_INFO("Sleeping done");
            return 1;
        }
        else
        {
            XBT_INFO("Sleeping until walltime");
            MSG_process_sleep(*remainingTime);
            XBT_INFO("Walltime reached");
            *remainingTime = 0;
            return 0;
        }
    }
    else if (strcmp(profile->type, "smpi") == 0)
        xbt_die("Profile with type %s is not yet implemented", profile->type);
    else
        xbt_die("Profile with type %s is not supported", profile->type);

    return 0;
}

int job_exec(int job_id, int nb_res, int *res_idxs, msg_host_t *nodes, double walltime)
{
    s_job_t * job = jobFromJobID(job_id);
    //XBT_INFO("job_exec: jobID %d, job=%p", job_id, job);

    msg_host_t * job_res = (msg_host_t *) malloc(nb_res * sizeof(s_msg_host_t));

    for(int i = 0; i < nb_res; i++)
        job_res[i] = nodes[res_idxs[i]];

    int ret = profile_exec(job->profile, job_id, nb_res, job_res, &walltime);

    free(job_res);
    return ret;
}


int job_exec1(int job_id, int nb_res, int *res_idxs, msg_host_t *nodes, double walltime,double coeff1,int casenum1,double coeff2, int casenum2,int cpu_com)
{
    s_job_t * job = jobFromJobID(job_id);

    msg_host_t * job_res = (msg_host_t *) malloc(nb_res * sizeof(s_msg_host_t));

    for(int i = 0; i < nb_res; i++)
        job_res[i] = nodes[res_idxs[i]];
        int ret = profile_exec1(job->profile, job_id, nb_res, job_res, &walltime, coeff1, casenum1,coeff2, casenum2, cpu_com);
    free(job_res);
    return ret;
}


int profile_exec1 (const char *profile_str, int job_id, int nb_res, msg_host_t *job_res, double * remainingTime,double coeff1,int casenum1,double coeff2, int casenum2, int cpu_com)
{
    double *computation_amount;
    double *communication_amount;
    double *coeffmatrix;
    msg_task_t ptask;
    int i,j = 0;
    profile_t profile = xbt_dict_get(profiles, profile_str);
    double copyvalue;

    if (strcmp(profile->type, "msg_par") == 0)
    {
        // These amounts are deallocated by SG
        computation_amount = malloc(nb_res * sizeof(double));
        communication_amount = malloc(nb_res * nb_res * sizeof(double));
        coeffmatrix = malloc (nb_res * sizeof(double));
        double *cpu = ((s_msg_par_t *)(profile->data))->cpu;
        double *com = ((s_msg_par_t *)(profile->data))->com;

        memcpy(computation_amount , cpu, nb_res * sizeof(double));
        memcpy(communication_amount, com, nb_res * nb_res * sizeof(double));
        printf("coeff1:  %f\n", coeff1);
       
        if(casenum1 == 1)
        {
            for (i = 0; i < nb_res; i++)
            {
                 if (computation_amount[i] != 0.000001)
                    computation_amount[i] = coeff1 * computation_amount[i];
                printf("%d changedcpu: %f\n",i, computation_amount[i]);
            }
        }
        if (casenum1 == 0)
        {
            for (i = 0; i < nb_res; i++)
            { 
                if (computation_amount[i] != 0.000001)
                    copyvalue = computation_amount[i];
                    computation_amount[i] = computation_amount[i] + coeff1;
                    printf("%d changedcpu: %f\n",i, computation_amount[i] );

                 if(cpu_com == 4)
                       coeffmatrix[i] = sqrt (computation_amount[i]/copyvalue);
            }
        }     
         if(casenum2 == 1)
        {
            for (i = 0; i < nb_res; i++)
            {
                if(cpu_com == 4 && casenum1 == 0)
                coeff2 = coeffmatrix[i];
                printf("coeff2:  %f\n", coeff2);
                for(j = 0;j < nb_res; j++)
                {
                    communication_amount[i*nb_res+j] = coeff2 * communication_amount[i*nb_res+j];
                    printf("changedcom: %lf\n",communication_amount[i*nb_res+j] );               
                }
            }
        }
       if (casenum2 == 0)
        {
            for (i = 0; i < nb_res; i++)
            {                
                for(j = 0;j < nb_res; j++)
                {
                    if (communication_amount[i*nb_res+j] > 0)
                        communication_amount[i*nb_res+j] = communication_amount[i*nb_res+j] + coeff2;
                    printf("%d changedcom: %lf\n",i,communication_amount[i*nb_res+j] );
                }
            }
        }
        char * tname = NULL;
        asprintf(&tname, "p %d", job_id);
        XBT_INFO("Creating task '%s'", tname);
        ptask = MSG_parallel_task_create(tname,
                                         nb_res, job_res,
                                         computation_amount,
                                         communication_amount, NULL);
        free(tname);
        free(coeffmatrix);
        // Let's spawn a process which will wait until walltime and cancel the task if needed
        KillerDelayData * killData = malloc(sizeof(KillerDelayData));
        killData->task = ptask;
        killData->walltime = *remainingTime;

        msg_process_t killProcess = MSG_process_create("killer", killerDelay, killData, MSG_host_self());

        double timeBeforeExecute = MSG_get_clock();
        XBT_INFO("Executing task '%s'", MSG_task_get_name(ptask));
        msg_error_t err = MSG_parallel_task_execute(ptask);
        *remainingTime = *remainingTime - (MSG_get_clock() - timeBeforeExecute);

        int ret = 1;
        if (err == MSG_OK)
            SIMIX_process_throw(killProcess, cancel_error, 0, "wake up");
        else if (err == MSG_TASK_CANCELED)
            ret = 0;
        else
            xbt_die("A task execution had been stopped by an unhandled way (err = %d)", err);

        XBT_INFO("Task '%s' finished", MSG_task_get_name(ptask));
        MSG_task_destroy(ptask);
        return ret;

    }

    else if (strcmp(profile->type, "msg_par_hg") == 0)
    {
        double cpu = ((s_msg_par_hg_t *)(profile->data))->cpu;
        double com = ((s_msg_par_hg_t *)(profile->data))->com;

        // These amounts are deallocated by SG
        computation_amount = malloc(nb_res * sizeof(double));
        communication_amount = malloc(nb_res * nb_res * sizeof(double));

        //XBT_DEBUG("msg_par_hg: nb_res: %d , cpu: %f , com: %f", nb_res, cpu, com);

        for (int i = 0; i < nb_res; i++)
            computation_amount[i] = cpu;

        for (int j = 0; j < nb_res; j++)
            for (int i = 0; i < nb_res; i++)
                communication_amount[nb_res * j + i] = com;

            
        char * tname = NULL;
        asprintf(&tname, "hg %d", job_id);
        XBT_INFO("Creating task '%s'", tname);

        ptask = MSG_parallel_task_create(tname,
                                         nb_res, job_res,
                                         computation_amount,
                                         communication_amount, NULL);
        free(tname);

        // Let's spawn a process which will wait until walltime and cancel the task if needed
        KillerDelayData * killData = malloc(sizeof(KillerDelayData));
        killData->task = ptask;
        killData->walltime = *remainingTime;

        msg_process_t killProcess = MSG_process_create("killer", killerDelay, killData, MSG_host_self());

        double timeBeforeExecute = MSG_get_clock();
        XBT_INFO("Executing task '%s'", MSG_task_get_name(ptask));
        msg_error_t err = MSG_parallel_task_execute(ptask);
        *remainingTime = *remainingTime - (MSG_get_clock() - timeBeforeExecute);

        int ret = 1;
        if (err == MSG_OK)
            SIMIX_process_throw(killProcess, cancel_error, 0, "wake up");
        else if (err == MSG_TASK_CANCELED)
            ret = 0;
        else
            xbt_die("A task execution had been stopped by an unhandled way (err = %d)", err);

        XBT_INFO("Task '%s' finished", MSG_task_get_name(ptask));
        MSG_task_destroy(ptask);
        return ret;
    }

    else if (strcmp(profile->type, "composed") == 0)
    {
        s_composed_prof_t * data = (s_composed_prof_t *) profile->data;
        int nb = data->nb;
        int lg_seq = data->lg_seq;
        char **seq = data->seq;

        for (int i = 0; i < nb; i++)
            for (int j = 0; j < lg_seq; j++)
                if (profile_exec1(seq[j], job_id, nb_res, job_res, remainingTime, coeff1, casenum1, coeff2, casenum2, cpu_com) == 0)
                    return 0;            
    }

    else if (strcmp(profile->type, "delay") == 0)
    {
        double delay = ((s_delay_t *)(profile->data))->delay;

        if (delay < *remainingTime)
        {
            XBT_INFO("Sleeping the whole task length");
            MSG_process_sleep(delay);
            XBT_INFO("Sleeping done");
            return 1;
        }
        else
        {
            XBT_INFO("Sleeping until walltime");
            MSG_process_sleep(*remainingTime);
            XBT_INFO("Walltime reached");
            *remainingTime = 0;
            return 0;
        }
    }

    else if (strcmp(profile->type, "smpi") == 0)
        xbt_die("Profile with type %s is not yet implemented", profile->type);
    
    else
        xbt_die("Profile with type %s is not supported", profile->type);

    return 0;
}


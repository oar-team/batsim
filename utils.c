/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */

#include <string.h>

#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(utils, "utils");

/**
 * \brief Load workload with jobs' profiles file
 */

int nb_jobs = 0;
xbt_dict_t profiles = NULL;
xbt_dynar_t jobs_dynar = NULL;
xbt_dict_t job_id_to_dynar_pos = NULL;

json_t *load_json_workload_profile(char *filename)
{
    json_t *root;
    json_error_t error;
    json_t *e;

    if (filename == NULL)
    {
        filename = "../workload_profiles/test_workload_profile.json";
    }

    root = json_load_file(filename, 0, &error);

    if(!root)
    {
        XBT_ERROR("error : root\n");
        XBT_ERROR("error : on line %d: %s\n", error.line, error.text);
        exit(1);
    }
    else
    {
        e = json_object_get(root, "description");

        if ( e != NULL )
            XBT_INFO("Json Profile and Workload File's description: \n%s", json_string_value(e));
        return root;
    }
}

double json_number_to_double(json_t *e)
{
    double value;

    if (json_typeof(e) == JSON_INTEGER)
        value = (double)json_integer_value(e);
    else
        value = (double)json_real_value(e);

    return value;
}

void retrieve_jobs(json_t *root) // todo: sort jobs by ascending submission time
{
    json_t *e;
    json_t *j;

    initializeJobStructures();
    e = json_object_get(root, "jobs");

    if (e != NULL)
    {
        nb_jobs = json_array_size(e);
        XBT_INFO("Json Workload with Profile File: nb_jobs %d", nb_jobs);
        
        for(int i = 0; i < nb_jobs; i++)
        {
            s_job_t * job = (s_job_t *) malloc(sizeof(s_job_t));

            j = json_array_get(e, i);
            job->id = json_integer_value(json_object_get(j,"id"));
            asprintf(&(job->id_str), "%d", job->id);

            job->submission_time = json_number_to_double(json_object_get(j,"subtime"));
            job->walltime = json_number_to_double(json_object_get(j,"walltime"));
            asprintf(&(job->profile), "%s", json_string_value(json_object_get(j,"profile"))); // todo: clean
            XBT_INFO("Read profile '%s' from job %d", job->profile, job->id);
            job->runtime = -1;
            job->nb_res = json_number_to_double(json_object_get(j,"res"));
            job->state = JOB_STATE_NOT_SUBMITTED;

            XBT_INFO("Read job: id=%d, subtime=%lf, walltime=%lf, profile='%s', nb_res=%d",
                     job->id, job->submission_time, job->walltime, job->profile, job->nb_res);

            if (!jobExists(job->id))
            {
                xbt_dynar_push(jobs_dynar, &job);

                int * insertPosition = (int *) malloc(sizeof(int));
                *insertPosition = xbt_dynar_length(jobs_dynar) - 1;
                xbt_dict_set(job_id_to_dynar_pos, job->id_str, insertPosition, free);
                XBT_INFO("Added to map '%s'->%d", job->id_str, *insertPosition);
            }
            else
            {
                XBT_WARN("Trying to insert job %d whereas it already exists. The new job is discarded.", job->id);
                freeJob(job);
            }
        }
    }
    else
    {
        XBT_INFO("Json Workload with Profile File: jobs array is missing !");
        exit(1);
    }

    /*XBT_INFO("Checking dynar values via foreach");
    int job_index = -1;
    s_job_t * job = NULL;

    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {
        //XBT_INFO("job_index=%d, job=%p", job_index, job);
        XBT_INFO("  id=%d, ptr=%p, submit_time=%lf, walltime=%lf, id_str='%s', profile='%s'",
                 job->id, job, job->submission_time, job->walltime, job->id_str, job->profile);
    }

    XBT_INFO("Checking dynar values by hand");
    int nb_jobs = xbt_dynar_length(jobs_dynar);
    for (int i = 0; i < nb_jobs; ++i)
    {
        s_job_t ** pjob = xbt_dynar_get_ptr(jobs_dynar, i);
        job = *pjob;
        XBT_INFO("  idx=%d, ptr=%p", i, job);
    }*/
}

void retrieve_profiles(json_t *root)
{
    json_t *j_profiles;
    const char *key;
    json_t *j_profile;
    json_t *e;
    int i = 0;
    profile_t profile;

    initializeJobStructures();

    j_profiles = json_object_get(root, "profiles");
    if ( j_profiles != NULL )
    {
        void *iter = json_object_iter(j_profiles);
        while(iter)
        {
            key = json_object_iter_key(iter);
            j_profile = json_object_iter_value(iter);

            profile = (profile_t) malloc(sizeof(s_profile_t));
            xbt_dict_set(profiles, key, profile, freeProfile);

            char * type = strdup(json_string_value(json_object_get(j_profile, "type")));
            profile->type = type;
            profile->data = NULL;

            if (strcmp(type, "msg_par") == 0)
            {
                s_msg_par_t * m_par = (s_msg_par_t *) malloc(sizeof(s_msg_par_t));
                profile->data = m_par;

                e = json_object_get(j_profile, "cpu");
                int nb_res = json_array_size(e);
                double *cpu = xbt_new0(double, nb_res);
                double *com = xbt_new0(double, nb_res * nb_res);

                for(i=0; i < nb_res; i++)
                    cpu[i] = (double)json_number_to_double( json_array_get(e,i) );

                e = json_object_get(j_profile, "com");
                for(i=0; i < nb_res * nb_res; i++)
                    com[i] = (double)json_number_to_double( json_array_get(e,i) );

                m_par->nb_res = nb_res;
                m_par->cpu = cpu;
                m_par->com = com;
            }
            else if (strcmp(type, "msg_par_hg") == 0)
            {
                s_msg_par_hg_t * m_par_hg = (s_msg_par_hg_t *) malloc(sizeof(s_msg_par_hg_t));
                profile->data = m_par_hg;

                e = json_object_get(j_profile, "cpu");
                m_par_hg->cpu = (double)json_number_to_double(e);
                e = json_object_get(j_profile, "com");
                m_par_hg->com = (double)json_number_to_double(e);
            }
            else if (strcmp(type, "composed") ==0)
            {
                s_composed_prof_t * composed = (s_composed_prof_t *) malloc(sizeof(s_composed_prof_t));
                profile->data = composed;

                e = json_object_get(j_profile, "nb");
                composed->nb = (int)json_integer_value(e);

                e = json_object_get(j_profile, "seq");
                int lg_seq = json_array_size(e);
                char ** seq = xbt_new(char*, lg_seq);

                for (i=0; i < lg_seq; i++)
                {
                    json_t * elem = json_array_get(e,i);
                    xbt_assert(json_is_string(elem), "In composed, the profile array must be made of strings");

                    seq[i] = strdup(json_string_value(elem));
                }

                composed->lg_seq = lg_seq;
                composed->seq = seq;
            }
            else if (strcmp(type, "delay") ==0)
            {
                s_delay_t * delay_prof = (s_delay_t *)malloc( sizeof(s_delay_t) );
                profile->data = delay_prof;

                e = json_object_get(j_profile, "delay");
                delay_prof->delay = (double)json_number_to_double(e);
            }
            else if (strcmp(type, "smpi") ==0)
            {
                XBT_WARN("Profile with type %s is not yet implemented", type);
            }
            else
            {
                XBT_WARN("Profile with type %s is not supported", profile->type);
            }

            iter = json_object_iter_next(j_profiles, iter);
        }
    }
    else
    {
        XBT_INFO("Json Workload with Profile File: profiles dict is missing !");
        exit(1);
    }
}

void freeProfile(void * profile)
{
    s_profile_t * prof = (s_profile_t *) profile;

    if (strcmp(prof->type, "msg_par") == 0)
    {
        s_msg_par_t * data = prof->data;

        xbt_free(data->cpu);
        xbt_free(data->com);
    }
    else if (strcmp(prof->type, "composed") == 0)
    {
        s_composed_prof_t * data = prof->data;

        for (int i = 0; i < data->lg_seq; ++i)
            free(data->seq[i]);

        xbt_free(data->seq);
    }

    free(prof->type);
    free(prof->data);
    free(prof);
}

void freeJob(void * job)
{
    s_job_t * j = (s_job_t *) job;

    free(j->id_str);
    free(j->profile);
    free(j);
}

void initializeJobStructures()
{
    static int alreadyCalled = 0;

    if (!alreadyCalled)
    {
        xbt_assert(jobs_dynar == NULL && job_id_to_dynar_pos == NULL && profiles == NULL);
        XBT_INFO("Creating job structures");

        jobs_dynar = xbt_dynar_new(sizeof(s_job_t*), NULL);
        job_id_to_dynar_pos = xbt_dict_new();
        profiles = xbt_dict_new();

        alreadyCalled = 1;
    }
}

void freeJobStructures()
{
    if (jobs_dynar != NULL)
    {
        XBT_INFO("Freeing jobs_dynar");

        unsigned int i;
        s_job_t * job;

        xbt_dynar_foreach(jobs_dynar, i, job)
        {
            freeJob(job);
        }

        xbt_dynar_free(&jobs_dynar);
        jobs_dynar = NULL;
    }

    if (job_id_to_dynar_pos != NULL)
    {
        XBT_INFO("Freeing jobs_id_to_dynar_pos");

        xbt_dict_free(&job_id_to_dynar_pos);
        job_id_to_dynar_pos = NULL;
    }

    if (profiles != NULL)
    {
        XBT_INFO("Freeing profiles");
        xbt_dict_free(&profiles);
        profiles = NULL;
    }
}

int jobExists(int jobID)
{
    char * jobName;
    asprintf(&jobName, "%d", jobID);

    int * dynarPosition = (int *) xbt_dict_get_or_null(job_id_to_dynar_pos, jobName);
    free (jobName);

    return dynarPosition != NULL;
}

s_job_t * jobFromJobID(int jobID)
{
    char * jobName;
    asprintf(&jobName, "%d", jobID);

    int * dynarPosition = (int *) xbt_dict_get_or_null(job_id_to_dynar_pos, jobName);
    xbt_assert(dynarPosition != NULL, "Invalid call: jobID %d does NOT exist", jobID);

    free(jobName);

    return *((s_job_t **) xbt_dynar_get_ptr(jobs_dynar, *dynarPosition));
}
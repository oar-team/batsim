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

/**
 * @brief Compares two jobs according to their submission time
 * @param[in] e1 The first job
 * @param[in] e2 The second job
 * @return An integer less than, equal to, or greater than zero if e1 is found, respectively, to be less than, to match, or be greater than e2
 */
static int job_submission_time_comparator(const void *e1, const void *e2)
{
   const s_job_t * j1 = *(s_job_t **) e1;
   const s_job_t * j2 = *(s_job_t **) e2;

   return j1->submission_time - j2->submission_time;
}

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

        if (e != NULL)
            XBT_INFO("Json Profile and Workload File's description:\n  %s", json_string_value(e));
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

void retrieve_jobs(json_t *root)
{
    json_t *e;
    json_t *j;

    initializeJobStructures();

    e = json_object_get(root, "jobs");
    xbt_assert(e != NULL, "Invalid JSON file: jobs array is missing");
    xbt_assert(json_typeof(e) == JSON_ARRAY, "Invalid JSON file: the 'jobs' field must be an array");

    nb_jobs = json_array_size(e);
    for(int i = 0; i < nb_jobs; i++)
    {
        s_job_t * job = xbt_new(s_job_t, 1);

        j = json_array_get(e, i);
        xbt_assert(json_typeof(j) == JSON_OBJECT);

        // jobID
        json_t * tmp = json_object_get(j, "id");
        xbt_assert(tmp != NULL, "Invalid JSON file: a job has no 'id' field");
        xbt_assert(json_typeof(tmp) == JSON_INTEGER, "Invalid JSON file: a job has a non-integral ID");
        job->id = json_integer_value(tmp);
        xbt_assert(job->id >= 0, "Invalid JSON file: a job has a negative id (%d)", job->id);
        xbt_assert(!jobExists(job->id), "Invalid JSON file: duplication of job %d", job->id);
        asprintf(&(job->id_str), "%d", job->id);

        // submission time
        tmp = json_object_get(j, "subtime");
        xbt_assert(tmp != NULL, "Invalid job %d from JSON file: it does not have a 'subtime' field.", job->id);
        xbt_assert(json_typeof(tmp) == JSON_INTEGER || json_typeof(tmp) == JSON_REAL, "Invalid job %d from JSON file: its 'subtime' field must be integral or real", job->id);
        job->submission_time = json_number_to_double(tmp);

        // walltime
        tmp = json_object_get(j, "walltime");
        xbt_assert(tmp != NULL, "Invalid job %d from JSON file: it does not have a 'walltime' field.", job->id);
        xbt_assert(json_typeof(tmp) == JSON_INTEGER || json_typeof(tmp) == JSON_REAL, "Invalid job %d from JSON file: its 'walltime' field must be integral or real", job->id);
        job->walltime = json_number_to_double(tmp);

        // number of resources
        tmp = json_object_get(j, "res");
        xbt_assert(tmp != NULL, "Invalid job %d from JSON file: it does not have a 'res' field.", job->id);
        xbt_assert(json_typeof(tmp) == JSON_INTEGER, "Invalid job %d from JSON file: its 'res' field must be integral", job->id);
        job->nb_res = json_integer_value(tmp);

        // profile
        tmp = json_object_get(j, "profile");
        xbt_assert(tmp != NULL, "Invalid job %d from JSON file: it does not have a 'profile' field.", job->id);
        xbt_assert(json_typeof(tmp) == JSON_STRING, "Invalid job %d from JSON file: its 'profile' field must be a string", job->id);
        asprintf(&(job->profile), "%s", json_string_value(tmp));

        job->startingTime = -1;
        job->runtime = -1;
        job->alloc_ids = 0;
        job->state = JOB_STATE_NOT_SUBMITTED;

        xbt_dynar_push(jobs_dynar, &job);
    }

    xbt_dynar_sort(jobs_dynar, job_submission_time_comparator);

    s_job_t * job;
    unsigned int job_index;

    // Let's create a mapping from the jobID to its position in the dynar
    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {
        int * insertPosition = xbt_new(int, 1);
        *insertPosition = job_index;
        xbt_dict_set(job_id_to_dynar_pos, job->id_str, insertPosition, free);

        xbt_assert(job == jobFromJobID(job->id));
    }

    XBT_INFO("%d jobs had been read from the JSON file", nb_jobs);
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
    xbt_assert(j_profiles != NULL, "Invalid JSON file: profiles dict is missing");
    xbt_assert(json_typeof(j_profiles) == JSON_OBJECT, "Invalid JSON file: the profiles must be a dict");

    void *iter = json_object_iter(j_profiles);
    while(iter)
    {
        key = json_object_iter_key(iter);
        xbt_assert(!profileExists(key), "Invalid JSON file: many profile share the name '%s'", key);
        j_profile = json_object_iter_value(iter);

        profile = (profile_t) malloc(sizeof(s_profile_t));
        xbt_dict_set(profiles, key, profile, freeProfile);

        const json_t * typeObject = json_object_get(j_profile, "type");
        xbt_assert(typeObject != NULL, "The profile '%s' has no 'type' field", key);
        xbt_assert(json_typeof(typeObject) == JSON_STRING, "The profile '%s' has a non-textual 'type' field", key);
        char * type;
        asprintf(&type, "%s", json_string_value(typeObject));
        profile->type = type;
        profile->data = NULL;

        if (strcmp(type, "msg_par") == 0)
        {
            s_msg_par_t * m_par = xbt_new(s_msg_par_t, 1);
            profile->data = m_par;

            e = json_object_get(j_profile, "cpu");
            xbt_assert(e != NULL, "The msg_par profile '%s' has no 'cpu' field", key);
            xbt_assert(json_typeof(e) == JSON_ARRAY, "The 'cpu' field of the msg_par profile '%s' must be an array", key);
            int nb_res = json_array_size(e);
            xbt_assert(nb_res > 0, "The 'cpu' field of the msg_par profile '%s' must be a non-empty array", key);
            double *cpu = xbt_new0(double, nb_res);
            double *com = xbt_new0(double, nb_res * nb_res);

            for(i=0; i < nb_res; i++)
            {
                json_t * elem = json_array_get(e,i);
                xbt_assert(json_typeof(elem) == JSON_INTEGER || json_typeof(elem) == JSON_REAL, "Invalid 'cpu' field of the msg_par profile '%s': content must only be integers or reals", key);
                cpu[i] = (double)json_number_to_double(json_array_get(e,i));
                xbt_assert(cpu[i] > 0, "Invalid 'cpu' field of the msg_par profile '%s': all values must be strictly greater than 0", key);
            }

            e = json_object_get(j_profile, "com");
            xbt_assert(e != NULL, "The msg_par profile '%s' has no 'com' field", key);
            xbt_assert(json_typeof(e) == JSON_ARRAY, "The 'com' field of the msg_par profile '%s' must be an array", key);
            xbt_assert(json_array_size(e) == (size_t)(nb_res * nb_res), "The 'com' array of the msg_par profile '%s' has an invalid size: it must be the square of the 'cpu' array size", key);
            for(i=0; i < nb_res * nb_res; i++)
            {
                com[i] = (double)json_number_to_double(json_array_get(e,i));
                xbt_assert(com[i] >= 0, "Invalid 'com' array of the msg_par profile '%s': all values must be greater than or equals to 0", key);
            }

            m_par->nb_res = nb_res;
            m_par->cpu = cpu;
            m_par->com = com;
        }
        else if (strcmp(type, "msg_par_hg") == 0)
        {
            s_msg_par_hg_t * m_par_hg = xbt_new(s_msg_par_hg_t, 1);
            profile->data = m_par_hg;

            e = json_object_get(j_profile, "cpu");
            xbt_assert(e != NULL, "The msg_par_hg profile '%s' has no 'cpu' field", key);
            xbt_assert(json_typeof(e) == JSON_INTEGER || json_typeof(e) == JSON_REAL, "The 'cpu' field of the msg_par_hg profile '%s' must be an integer or a real", key);
            m_par_hg->cpu = (double)json_number_to_double(e);
            xbt_assert(m_par_hg->cpu > 0, "The 'cpu' field of the msg_par_hg profile '%s' must be strictly positive", key);

            e = json_object_get(j_profile, "com");
            xbt_assert(e != NULL, "The msg_par_hg profile '%s' has no 'com' field", key);
            xbt_assert(json_typeof(e) == JSON_INTEGER || json_typeof(e) == JSON_REAL, "The 'com' field of the msg_par_hg profile '%s' must be an integer or a real", key);
            m_par_hg->com = (double)json_number_to_double(e);
            xbt_assert(m_par_hg->com >= 0, "The 'com' field of the msg_par_hg profile '%s' must be positive", key);
        }
        else if (strcmp(type, "composed") == 0)
        {
            s_composed_prof_t * composed = xbt_new(s_composed_prof_t, 1);
            profile->data = composed;

            e = json_object_get(j_profile, "nb");
            xbt_assert(e != NULL, "The composed profile '%s' must have a 'nb' field", key);
            xbt_assert(json_typeof(e) == JSON_INTEGER || json_typeof(e) == JSON_REAL, "The 'nb' field of the composed profile '%s' must be an integer or a real", key);
            composed->nb = (int)json_integer_value(e);
            xbt_assert(composed->nb > 0, "Invalid composed profile '%s': the 'nb' field must be strictly positive", key);

            e = json_object_get(j_profile, "seq");
            xbt_assert(e != NULL, "The composed profile '%s' must have a 'seq' field", key);
            xbt_assert(json_typeof(e) == JSON_ARRAY, "The composed profile '%s' must have an array as a 'seq' field", key);
            int lg_seq = json_array_size(e);
            xbt_assert(lg_seq > 0, "The 'seq' field of the composed profile '%s' must be a non-empty array", key);
            char ** seq = xbt_new(char*, lg_seq);

            for (i=0; i < lg_seq; i++)
            {
                json_t * elem = json_array_get(e,i);
                xbt_assert(json_is_string(elem), "Invalid 'seq' field of the composed profile '%s': all its elements must be strings", key);
                asprintf(&(seq[i]), "%s", json_string_value(elem));
            }

            composed->lg_seq = lg_seq;
            composed->seq = seq;
        }
        else if (strcmp(type, "delay") == 0)
        {
            s_delay_t * delay_prof = xbt_new(s_delay_t, 1);
            profile->data = delay_prof;

            e = json_object_get(j_profile, "delay");
            xbt_assert(e != NULL, "The delay profile '%s' must have a 'delay' field", key);
            xbt_assert(json_typeof(e) == JSON_INTEGER || json_typeof(e) == JSON_REAL, "The 'delay' field of the delay profile '%s' must be an integer or a real", key);
            delay_prof->delay = (double)json_number_to_double(e);
            xbt_assert(delay_prof->delay > 0, "The 'delay' field of the delay profile '%s' must be strictly positive", key);
        }
        else if (strcmp(type, "smpi") == 0)
        {
            XBT_WARN("Profile with type %s is not yet implemented", type);
        }
        else
            xbt_die("Invalid profile '%s' : type '%s' is not supported", key, profile->type);

        iter = json_object_iter_next(j_profiles, iter);
    }

    XBT_INFO("%d profiles had been read from the JSON file", xbt_dict_size(profiles));
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
    free(j->alloc_ids);
    free(j);
}

void initializeJobStructures()
{
    static int alreadyCalled = 0;

    if (!alreadyCalled)
    {
        xbt_assert(jobs_dynar == NULL && job_id_to_dynar_pos == NULL && profiles == NULL);
        //XBT_INFO("Creating job structures");

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
        //XBT_INFO("Freeing jobs_dynar");

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
        //XBT_INFO("Freeing jobs_id_to_dynar_pos");

        xbt_dict_free(&job_id_to_dynar_pos);
        job_id_to_dynar_pos = NULL;
    }

    if (profiles != NULL)
    {
        //XBT_INFO("Freeing profiles");
        xbt_dict_free(&profiles);
        profiles = NULL;
    }
}

int jobExists(int jobID)
{
    char * jobName;
    asprintf(&jobName, "%d", jobID);

    int * dynarPosition = (int *) xbt_dict_get_or_null(job_id_to_dynar_pos, jobName);
    free(jobName);

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

int profileExists(const char * profileName)
{
    s_profile_t * profile = xbt_dict_get_or_null(profiles, profileName);
    return profile != NULL;
}


void checkJobsAndProfilesValidity()
{
    // Let's check that every composed profile points to existing profiles
    s_profile_t * profile = NULL;
    xbt_dict_cursor_t dict_cursor = NULL;
    const char * profile_name = NULL;
    xbt_dict_foreach(profiles, dict_cursor, profile_name, profile)
    {
        if (strcmp(profile->type, "composed") == 0)
        {
            s_composed_prof_t * comp = profile->data;
            for (int i = 0; i < comp->lg_seq; ++i)
            {
                xbt_assert(profileExists(comp->seq[i]), "Invalid composed profile '%s': the used profile '%s' does not exist", profile_name, comp->seq[i]);
                // todo: check that there are no circular calls between composed profiles...
                // todo: compute the constraint of the profile number of resources, to check if it match the jobs that use it
            }
        }
    }

    // Let's check that the profile of each job exists
    s_job_t * job;
    unsigned int job_index;
    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {
        xbt_assert(profileExists(job->profile), "Invalid job %d: the associated profile '%s' does not exist", job->id, job->profile);
        s_profile_t * prof = xbt_dict_get_or_null(profiles, job->profile);

        if (strcmp(prof->type, "msg_par") == 0)
        {
            s_msg_par_t * data = prof->data;
            xbt_assert(data->nb_res == job->nb_res, "Invalid job %d: the requested number of resources (%d) do NOT match"
                       "the number of resources of the associated profile '%s' (%d)", job->id, job->nb_res, job->profile, data->nb_res);
        }
        else if (strcmp(prof->type, "composed") == 0)
        {
            // todo: check if the number of resources matches a resource-constrained composed profile
        }
    }
}

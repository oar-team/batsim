/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */
#ifndef _INCL_GUARD_UTIL
#define _INCL_GUARD_UTIL
#endif

#include <simgrid/msg.h>
#include <xbt.h>
#include <xbt/sysdep.h>
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <jansson.h> /* json parsing */

#include "job.h"

//! The structure used to store a profile
typedef struct s_profile
{
    char * type; //! The profile type
    void * data; //! The profile data
} s_profile_t, *profile_t;

// Global variables
//! The number of jobs
extern int nb_jobs;
//! The number of profiles
extern xbt_dict_t profiles;
//! The jobs dynamic array
extern xbt_dynar_t jobs_dynar;
//! The dictionary associating job IDs to their position in jobs_dynar
extern xbt_dict_t job_id_to_dynar_pos;

// Functions
/**
 * @brief Converts a JSON number to a double
 * @param[in] e The JSON node
 * @return The double value corresponding to e
 */
double json_number_to_double(json_t *e);

/**
 * @brief Loads the workload and the profiles from a given filename.
 * @param[in] filename The filename
 * @return The JSON root of what had been read (must be freed later)
 */
json_t *load_json_workload_profile(char *filename);

/**
 * @brief Loads the jobs from a JSON root
 * @param[in] root The JSON root
 */
void retrieve_jobs(json_t *root);

/**
 * @brief Loads the profiles from a JSON root
 * @param[in] root The JSON root
 */
void retrieve_profiles(json_t *root);

/**
 * @brief Frees a profile
 * @param[in,out] profile The profile
 */
void freeProfile(void * profile);

/**
 * @brief Frees a job
 * @param[in, out] job The job
 */
void freeJob(void * job);

/**
 * @brief Initializes the job structures.
 */
void initializeJobStructures();
/**
 * @brief Frees the job structures
 */
void freeJobStructures();

/**
 * @brief Checks whether a job exist or not.
 * @param[in] jobID The ID of the job to seek
 * @return TRUE if the job exists, FALSE otherwise
 */
int jobExists(int jobID);

/**
 * @brief Returns the job corresponding to a given job ID
 * @param[in] jobID The job ID
 * @return The job corresponding to jobID. The existence of the job is asserted in this function.
 */
s_job_t * jobFromJobID(int jobID);

/**
 * @brief Checks whether jobs and profiles are valid or not
 */
void checkJobsAndProfilesValidity();

/**
 * @brief Checks whether a profile exist or not
 * @param[in] profileName The profile name
 * @return TRUE if the profile exists, FALSE otherwise
 */
int profileExists(const char * profileName);

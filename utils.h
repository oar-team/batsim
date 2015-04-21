/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */ 
#pragma once

#include <simgrid/msg.h>
#include <xbt.h>
#include <xbt/sysdep.h>
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <jansson.h> /* json parsing */

#include "job.h"

typedef struct s_profile
{
	char * type;
	void * data;
} s_profile_t, *profile_t;

// Global variables
extern int nb_jobs;
extern xbt_dict_t profiles;
extern xbt_dynar_t jobs_dynar;
extern xbt_dict_t job_id_to_dynar_pos;

// Functions
double json_number_to_double(json_t *e);
json_t *load_json_workload_profile(char *filename);
void retrieve_jobs(json_t *root);
void retrieve_profiles(json_t *root);
void freeProfile(void * profile);
void freeJob(void * job);
void initializeJobStructures();
void freeJobStructures();

int jobExists(int jobID);
s_job_t * jobFromJobID(int jobID);

void checkJobsAndProfilesValidity();

int profileExists(const char * profileName);

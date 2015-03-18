/* Copyright (c) 2015. The OAR Team.
 * All rights reserved.                       */ 
#pragma once

#include <msg/msg.h>
#include <xbt.h>
#include <xbt/sysdep.h>
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <jansson.h> /* json parsing */

typedef struct s_profile
{
	const char *type;
	void *data;
} s_profile_t, *profile_t;

// Global variables
extern int nb_jobs;
extern s_job_t * jobs;
extern xbt_dict_t profiles;
extern xbt_dict_t jobs_idx2id;
extern xbt_dynar_t jobs_dynar;

// Functions
double json_number_to_double(json_t *e);
json_t *load_json_workload_profile(char *filename);
void retrieve_jobs(json_t *root);
void retrieve_profiles(json_t *root);
void freeProfile(void * profile);
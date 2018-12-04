#!/usr/bin/env python3

"""Transforms a Batsim _jobs.csv output into a SWF."""

# Dependency : sortedcontainers
#    - installation: pip install sortedcontainers
# Everything else should be in the standard library
# Tested on cpython 3.4.3

import argparse
import csv

from job import Job

parser = argparse.ArgumentParser(description='Reads a CSV Batsim jobs output '
                                 'file and transforms it into a SWF '
                                 '(Standard Workload Format) file')
parser.add_argument('inputCSV', type=argparse.FileType('r'),
                    help='The input CSV file')
parser.add_argument('outputSWF', type=argparse.FileType(
    'w'), help='The output SWF file')

args = parser.parse_args()

#######################
# Input CSV traversal #
#######################
jobs = {}
reader = csv.DictReader(args.inputCSV)

for row in reader:
    job_id = int(row["job_id"])
    submit_time = float(row["submission_time"])
    nb_res = int(row["requested_number_of_resources"])
    wall_time = float(row["requested_time"])
    run_time = float(row["execution_time"])
    wait_time = float(row["waiting_time"])

    success = bool(int(row["success"]) == 1)

    if success and (nb_res > 0):
        job = Job(job_id=job_id,
                  nb_res=nb_res,
                  wait_time=wait_time,
                  run_time=run_time,
                  submit_time=submit_time,
                  wall_time=wall_time)

        jobs[job_id] = job

#####################
# Output SWF export #
#####################

writer = csv.DictWriter(args.outputSWF,
                        delimiter=' ',
                        fieldnames=['job_id',
                                    'submit_time',
                                    'wait_time',
                                    'run_time',
                                    'nb_res_used',
                                    'avg_cputime',
                                    'used_memory',
                                    'nb_res_requested',
                                    'walltime',
                                    'requested_memory',
                                    'status',
                                    'user_id',
                                    'group_id',
                                    'exec_id',
                                    'queue_id',
                                    'partition_id',
                                    'preceding_job_id',
                                    'think_time_from_preceding_job'])

for job_id in jobs:
    job = jobs[job_id]

    d = {'job_id': job.job_id,
         'submit_time': job.submit_time,
         'wait_time': job.wait_time,
         'run_time': job.run_time,
         'nb_res_used': job.nb_res,
         'avg_cputime': -1,
         'used_memory': -1,
         'nb_res_requested': job.nb_res,
         'walltime': job.wall_time,
         'requested_memory': -1,
         'status': 1,
         'user_id': -1,
         'group_id': -1,
         'exec_id': -1,
         'queue_id': -1,
         'partition_id': -1,
         'preceding_job_id': -1,
         'think_time_from_preceding_job': -1}

    writer.writerow(d)

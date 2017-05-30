#!/usr/bin/env python3

"""Transforms a SWF to a Batsim _jobs.csv output."""

# Dependency : sortedcontainers
#    - installation: pip install sortedcontainers
# Everything else should be in the standard library
# Tested on cpython 3.4.3

from sortedcontainers import SortedSet
import argparse
import re
import csv

from common import take_first_resources
from job import Job
from swf import SwfField


parser = argparse.ArgumentParser(
    description='Reads a SWF (Standard Workload Format) file and transform it into a CSV Batsim jobs output')
parser.add_argument('inputSWF', type=argparse.FileType('r'),
                    help='The input SWF file')
parser.add_argument('outputCSV', type=argparse.FileType(
    'w'), help='The output CSV file')
parser.add_argument('nb_res', type=int,
                    help='The number of resources the workload has been executed on')

group = parser.add_mutually_exclusive_group()
group.add_argument("-v", "--verbose", action="store_true")
group.add_argument("-q", "--quiet", action="store_true")

args = parser.parse_args()

assert(args.nb_res > 0), "Invalid nb_res paramter"


element = '([-+]?\d+(?:\.\d+)?)'
r = re.compile('\s*' + (element + '\s+') * 17 + element + '\s*')

currentID = 0
version = 0


jobs = {}

# Let's loop over the lines of the input file
for line in args.inputSWF:
    res = r.match(line)

    if res:
        job_id = (int(float(res.group(SwfField.JOB_ID.value))))
        nb_res = int(
            float(res.group(SwfField.ALLOCATED_PROCESSOR_COUNT.value)))
        wait_time = float(res.group(SwfField.WAIT_TIME.value))
        run_time = float(res.group(SwfField.RUN_TIME.value))
        submit_time = max(0, float(res.group(SwfField.SUBMIT_TIME.value)))
        wall_time = max(run_time, float(
            res.group(SwfField.REQUESTED_TIME.value)))

        if nb_res > 0 and wall_time > run_time and run_time > 0 and submit_time >= 0:
            job = Job(job_id=job_id,
                      nb_res=nb_res,
                      wait_time=wait_time,
                      run_time=run_time,
                      submit_time=submit_time,
                      wall_time=wall_time)

            jobs[job_id] = job

##############
# Simulation #
##############

# Let's simulate where the jobs should have been placed by a simple
# "take first resources" policy. Let's create a list of events
events = []

for job_id in jobs:
    job = jobs[job_id]

    start_event = (float(job.start_time), '1:start', job_id)
    finish_event = (float(job.finish_time), '0:finish', job_id)

    events.append(start_event)
    events.append(finish_event)

events.sort()

for i in range(len(events) - 1):
    curr_event = events[i]
    next_event = events[i + 1]
    assert(curr_event[0] <= next_event[0])

# Let's traverse the list of events to associate resources to jobs
available_res = SortedSet(range(args.nb_res))

for event in events:
    print('nb_res_available : ', len(available_res))

    (date, state, job_id) = event
    job = jobs[job_id]
    if state == '1:start':
        prev_len = len(available_res)
        (new_available, alloc) = take_first_resources(available_res, job.nb_res)
        available_res = new_available
        job.resources = alloc
        assert(len(job.resources) == job.nb_res), "{}, expected {}".format(
            job.resources, job.nb_res)
        assert(len(available_res) == prev_len - job.nb_res), "{}, expected {}".format(
            len(available_res), prev_len - job.nb_res)
    elif state == '0:finish':
        available_res.update(job.resources)

##############
# Export CSV #
##############

writer = csv.DictWriter(args.outputCSV,
                        fieldnames=["job_id",
                                    "submission_time",
                                    "requested_number_of_processors",
                                    "requested_time",
                                    "success",
                                    "starting_time",
                                    "execution_time",
                                    "finish_time",
                                    "waiting_time",
                                    "turnaround_time",
                                    "stretch",
                                    "consumed_energy",
                                    "allocated_processors"])
writer.writeheader()

for job_id in jobs:
    job = jobs[job_id]

    d = {"job_id": job.job_id,
         "submission_time": job.submit_time,
         "requested_number_of_processors": job.nb_res,
         "requested_time": job.wall_time,
         "success": 1,
         "starting_time": job.start_time,
         "execution_time": job.run_time,
         "finish_time": job.finish_time,
         "waiting_time": job.wait_time,
         "turnaround_time": job.turnaround_time,
         "stretch": job.stretch,
         "consumed_energy": -1,
         "allocated_processors": ' '.join(str(res) for res in job.resources)
         }

    writer.writerow(d)

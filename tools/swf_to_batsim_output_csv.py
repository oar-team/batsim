#!/usr/bin/python3

# Dependency : sortedcontainers
#    - installation: pip install sortedcontainers
# Everything else should be in the standard library
# Tested on cpython 3.4.3

from enum import Enum, unique
from sortedcontainers import SortedSet
import argparse
import re
import csv
import sys
import datetime
import random

class Job:
	def __init__(self,
	             job_id,
	             nb_res,
	             wait_time,
	             run_time,
	             submit_time,
	             wall_time):
		self.job_id = job_id
		self.nb_res = nb_res
		self.wait_time = wait_time
		self.run_time = run_time
		self.submit_time = submit_time
		self.wall_time = wall_time

		self.start_time = self.submit_time + self.wait_time
		self.finish_time = self.start_time + self.run_time
		self.turnaround_time = self.finish_time - self.submit_time
		self.stretch = self.turnaround_time / self.nb_res

		self.resources = SortedSet()

def take_first_resources(available_res, nb_res_to_take):
	assert(len(available_res) >= nb_res_to_take), 'Invalid schedule. Want to use {} resources whereas only {} are available ({})'.format(
		nb_res_to_take, len(available_res), available_res)
	allocation = SortedSet()
	l = list(available_res)

	for i in range(nb_res_to_take):
		allocation.add(l[i])

	available_res.difference_update(allocation)

	if len(available_res) > 0:
		min_available_res = min(available_res)
		for res in allocation:
			assert(res < min_available_res), "Invalid sort"

	return (available_res, allocation)

@unique
class SwfField(Enum):
	JOB_ID=1
	SUBMIT_TIME=2
	WAIT_TIME=3
	RUN_TIME=4
	ALLOCATED_PROCESSOR_COUNT=5
	AVERAGE_CPU_TIME_USED=6
	USED_MEMORY=7
	REQUESTED_NUMBER_OF_PROCESSORS=8
	REQUESTED_TIME=9
	REQUESTED_MEMORY=10
	STATUS=11
	USER_ID=12
	GROUP_ID=13
	APPLICATION_ID=14
	QUEUD_ID=15
	PARTITION_ID=16
	PRECEDING_JOB_ID=17
	THINK_TIME_FROM_PRECEDING_JOB=18

parser = argparse.ArgumentParser(description='Reads a SWF (Standard Workload Format) file and transform it into a CSV Batsim jobs output')
parser.add_argument('inputSWF', type=argparse.FileType('r'), help='The input SWF file')
parser.add_argument('outputCSV', type=argparse.FileType('w'), help='The output CSV file')
parser.add_argument('nb_res', type=int, help='The number of resources the workload has been executed on')

group = parser.add_mutually_exclusive_group()
group.add_argument("-v", "--verbose", action="store_true")
group.add_argument("-q", "--quiet", action="store_true")

args = parser.parse_args()

assert(args.nb_res > 0), "Invalid nb_res paramter"


element = '([-+]?\d+(?:\.\d+)?)'
r = re.compile('\s*' + (element + '\s+') * 17 + element + '\s*')

currentID = 0
version=0


jobs = {}

# Let's loop over the lines of the input file
for line in args.inputSWF:
	res = r.match(line)

	if res:
		job_id = (int(float(res.group(SwfField.JOB_ID.value))))
		nb_res = int(float(res.group(SwfField.ALLOCATED_PROCESSOR_COUNT.value)))
		wait_time = float(res.group(SwfField.WAIT_TIME.value))
		run_time = float(res.group(SwfField.RUN_TIME.value))
		submit_time = max(0,float(res.group(SwfField.SUBMIT_TIME.value)))
		wall_time = max(run_time, float(res.group(SwfField.REQUESTED_TIME.value)))

		if nb_res > 0 and wall_time > run_time and run_time > 0 and submit_time >= 0:
			job = Job(job_id = job_id,
					  nb_res = nb_res,
					  wait_time = wait_time,
					  run_time = run_time,
					  submit_time = submit_time,
					  wall_time = wall_time)

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
	next_event = events[i+1]
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

	d = {"job_id" : job.job_id,
		 "submission_time" : job.submit_time,
		 "requested_number_of_processors" : job.nb_res,
		 "requested_time" : job.wall_time,
		 "success" : 1,
		 "starting_time" : job.start_time,
		 "execution_time" : job.run_time,
		 "finish_time" : job.finish_time,
		 "waiting_time" : job.wait_time,
		 "turnaround_time" : job.turnaround_time,
		 "stretch" : job.stretch,
		 "consumed_energy" : -1,
		 "allocated_processors" : ' '.join(str(res) for res in job.resources)
	}

	writer.writerow(d)

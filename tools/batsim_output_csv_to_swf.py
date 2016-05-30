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

parser = argparse.ArgumentParser(description='Reads a CSV Batsim jobs output file and transforms it into a SWF (Standard Workload Format) file')
parser.add_argument('inputCSV', type=argparse.FileType('r'), help='The input CSV file')
parser.add_argument('outputSWF', type=argparse.FileType('w'), help='The output SWF file')

args = parser.parse_args()

#######################
# Input CSV traversal #
#######################
jobs = {}
reader = csv.DictReader(args.inputCSV)

for row in reader:
	job_id = int(row["jobID"])
	submit_time = float(row["submission_time"])
	nb_res = int(row["requested_number_of_processors"])
	wall_time = float(row["requested_time"])
	run_time = float(row["execution_time"])
	wait_time = float(row["waiting_time"])

	success = bool(int(row["success"]) == 1)

	if success and (nb_res > 0):
		job = Job(job_id = job_id,
	              nb_res = nb_res,
	              wait_time = wait_time,
	              run_time = run_time,
	              submit_time = submit_time,
	              wall_time = wall_time)

		jobs[job_id] = job

#####################
# Output SWF export #
#####################

writer = csv.DictWriter(args.outputSWF,
                        delimiter = ' ',
                        fieldnames = ['job_id',
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

	d = {'job_id' : job.job_id,
         'submit_time' : job.submit_time,
         'wait_time' : job.wait_time,
         'run_time' : job.run_time,
         'nb_res_used' : job.nb_res,
         'avg_cputime' : -1,
         'used_memory' : -1,
         'nb_res_requested' : job.nb_res,
         'walltime' : job.wall_time,
         'requested_memory' : -1,
         'status' : 1,
         'user_id' : -1,
         'group_id' : -1,
         'exec_id' : -1,
         'queue_id' : -1,
         'partition_id' : -1,
         'preceding_job_id' : -1,
         'think_time_from_preceding_job' : -1}

	writer.writerow(d)

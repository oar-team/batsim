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
import json
import sys
import datetime
import random

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

parser = argparse.ArgumentParser(description='Reads a SWF (Standard Workload Format) file and transform it into a JSON Batsim workload (with delay jobs)')
parser.add_argument('inputSWF', type=argparse.FileType('r'),
                    help='The input SWF file')
parser.add_argument('outputJSON', type=str, help='The output JSON file')
parser.add_argument('-jwf', '--jobWalltimeFactor',
                    type=float, default=2,
                    help='Jobs walltimes are computed by the formula max(givenWalltime, jobWalltimeFactor*givenRuntime)')
parser.add_argument('-gwo', '--givenWalltimeOnly',
                    action="store_true",
                    help='If set, only the given walltime in the trace will be used')
parser.add_argument('-jg', '--jobGrain',
                    type=int, default=1,
                    help='Selects the level of detail we want for jobs. This parameter is used to group jobs that have close running time')
parser.add_argument('-pf', '--platformSize', type=int, default=None,
                    help='If set, the number of machines to put in the output JSON files is set by this parameter instead of taking the maximum job size')
parser.add_argument('-i', '--indent', type=int, default=None,
                    help='If set to a non-negative integer, then JSON array elements and object members will be pretty-printed with that indent level. An indent level of 0, or negative, will only insert newlines. The default value (None) selects the most compact representation.')
parser.add_argument('-t', '--translate_submit_times',
                    action="store_true",
                    help="If set, the jobs' submit times will be translated towards 0")

group = parser.add_mutually_exclusive_group()
group.add_argument("-v", "--verbose", action="store_true")
group.add_argument("-q", "--quiet", action="store_true")

args = parser.parse_args()


element = '([-+]?\d+(?:\.\d+)?)'
r = re.compile('\s*' + (element + '\s+') * 17 + element + '\s*')

currentID = 0
version=0

# Let a job be a tuple (job_id, nb_res, run_time, submit_time, profile, walltime)

jobs = []
profiles = SortedSet()

nb_jobs_discarded = 0
minimum_observed_submit_time = float('inf')

# Let's loop over the lines of the input file
for line in args.inputSWF:
	res = r.match(line)

	if res:
		job_id = (int(float(res.group(SwfField.JOB_ID.value))))
		nb_res = int(float(res.group(SwfField.ALLOCATED_PROCESSOR_COUNT.value)))
		run_time = float(res.group(SwfField.RUN_TIME.value))
		submit_time = max(0,float(res.group(SwfField.SUBMIT_TIME.value)))
		walltime = max(args.jobWalltimeFactor*run_time,
		               float(res.group(SwfField.REQUESTED_TIME.value)))

		if args.givenWalltimeOnly:
			walltime = float(res.group(SwfField.REQUESTED_TIME.value))

		if nb_res > 0 and walltime > run_time and run_time > 0 and submit_time >= 0:
			profile = int(((run_time // args.jobGrain)+1) * args.jobGrain)
			profiles.add(profile)

			job = (currentID, nb_res, run_time, submit_time, profile, walltime)
			currentID = currentID + 1
			minimum_observed_submit_time = min(minimum_observed_submit_time,
			                                   submit_time)
			jobs.append(job)
		else:
			nb_jobs_discarded += 1
			if args.verbose:
				print('Job {} has been discarded'.format(job_id))

# Export JSON
# Let's generate a list of dictionaries for the jobs
djobs = list()
for (job_id, nb_res, run_time, submit_time, profile, walltime) in jobs:
	djobs.append({'id':job_id,
	             'subtime':submit_time - minimum_observed_submit_time,
	             'walltime':walltime, 'res':nb_res,
	             'profile': str(profile)})

# Let's generate a dict of dictionaries for the profiles
dprofs = {}
for profile in profiles:
	dprofs[str(profile)] = {'type':'delay', 'delay':profile}

platform_size = max([nb_res for (job_id, nb_res, run_time, submit_time,
                                 profile, walltime) in jobs])
if args.platformSize != None:
	if args.platformSize < 1:
		print('Invalid input: platform size must be strictly positive')
		exit(1)
	platform_size = args.platformSize

data = {
	'version':version,
	'command':' '.join(sys.argv[:]),
	'date': datetime.datetime.now().isoformat(' '),
	'description':'this workload had been automatically generated',
	'nb_res': platform_size,
	'jobs':djobs,
	'profiles':dprofs }

try:
	outFile = open(args.outputJSON, 'w')
	json.dump(data, outFile, indent=args.indent, sort_keys=True)

	if not args.quiet:
		print('{} jobs and {} profiles had been created'.format(len(jobs),
		                                                        len(profiles)))
		print('{} jobs have been discarded'.format(nb_jobs_discarded))

except IOError:
	print('Cannot write file', outputJsonFilename)

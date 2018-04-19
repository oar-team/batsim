#!/usr/bin/env python3
import json
import numpy
import sys
import pandas as pd

def check_present_bool(row):
    if row['metadata'] == "":
        return False

    if isinstance(row['metadata'], float) or isinstance(row['metadata'], numpy.float64):
        if not numpy.isfinite(row['metadata']):
            return False

    return True

def check_present(row):
    return int(check_present_bool(row))

def check_absent(row):
    return int(not check_present_bool(row))

# Beginning of script
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))

sched_input = json.load(open('{output_directory}/sched_input.json'.format(
    output_directory=sys.argv[1].replace('/out', ''))))
set_metadata = sched_input['set_job_metadata']

if set_metadata:
    jobs['valid'] = jobs.apply(check_present, axis=1)
else:
    jobs['valid'] = jobs.apply(check_absent, axis=1)

if jobs['valid'].sum() != len(jobs):
    print('Some jobs are invalid!')
    print(jobs[['job_id', 'metadata', 'valid']])
    sys.exit(1)
else:
    print('All jobs are valid!')
    print(jobs[['job_id', 'metadata', 'valid']])
    sys.exit(0)

#!/usr/bin/env python3
import sys
import pandas as pd

def check_ok_bool(row):
    # Walltime is set and the job execution time is greater than its walltime
    if row['execution_time'] > row['requested_time'] and row['requested_time'] != -1:
        return False

    # Success -> (finished before walltime OR no walltime set)
    if row['success']:
        return (row['execution_time'] < row['requested_time']) or (row['requested_time'] == -1)
    # Failure -> walltime reached
    return row['execution_time'] >= row['requested_time']

def check_ok(row):
    return int(check_ok_bool(row))

# Beginning of script
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))

jobs['valid'] = jobs.apply(check_ok, axis=1)
if jobs['valid'].sum() != len(jobs):
    print('Some jobs are invalid!')
    print(jobs[['job_id', 'requested_time', 'execution_time', 'success',
                'valid']])
    sys.exit(1)
else:
    print('All jobs are valid!')
    print(jobs[['job_id', 'requested_time', 'execution_time', 'success',
                'valid']])
    sys.exit(0)

#!/usr/bin/env python3
import numpy
import sys
import pandas as pd

def check_ok_bool(row):
    if row['metadata'] == "":
        return False

    if isinstance(row['metadata'], float) or isinstance(row['metadata'], numpy.float64):
        if not numpy.isfinite(row['metadata']):
            return False

    return True

def check_ok(row):
    return int(check_ok_bool(row))

# Beginning of script
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))

jobs['valid'] = jobs.apply(check_ok, axis=1)
if jobs['valid'].sum() != len(jobs):
    print('Some jobs are invalid!')
    print(jobs[['job_id', 'metadata', 'valid']])
    sys.exit(1)
else:
    print('All jobs are valid!')
    print(jobs[['job_id', 'metadata', 'valid']])
    sys.exit(0)

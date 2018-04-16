#!/usr/bin/env python3
import sys
import pandas as pd

# Let's get when jobs have been released
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))

epsilon = 0.1
max_runtime = 10 + epsilon

jobs_too_long = jobs.loc[jobs['execution_time'] > max_runtime]

if jobs_too_long.empty:
    print('All jobs have been shorter than {} s'.format(max_runtime))
    sys.exit(0)
else:
    print('Some jobs were longer than {} s'.format(max_runtime))
    print(jobs_too_long)
    sys.exit(1)

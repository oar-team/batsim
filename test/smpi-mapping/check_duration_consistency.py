#!/usr/bin/env python3
import sys
import pandas as pd

# Beginning of script
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))

# Jobs 1 and 2 should have similar execution times
a = 0
b = 1
if abs(jobs['execution_time'][a] - jobs['execution_time'][b]) > 1:
    print("Jobs {id1} and {id2} should have similar runtimes, but "
          "they have not ({runtime1}, {runtime2}).".format(
        id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
        runtime1= jobs['execution_time'][a],
        runtime2= jobs['execution_time'][b]))
    sys.exit(1)
else:
    print("OK. Jobs {id1} and {id2} have similar runtimes "
          "({runtime1}, {runtime2}).".format(
        id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
        runtime1= jobs['execution_time'][a],
        runtime2= jobs['execution_time'][b]))

# Jobs 3 and 4 should have similar execution times
a = 2
b = 3
if abs(jobs['execution_time'][a] - jobs['execution_time'][b]) > 1:
    print("Jobs {id1} and {id2} should have similar runtimes, but "
          "they have not ({runtime1}, {runtime2}).".format(
        id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
        runtime1= jobs['execution_time'][a],
        runtime2= jobs['execution_time'][b]))
    sys.exit(1)
else:
    print("OK. Jobs {id1} and {id2} have similar runtimes "
          "({runtime1}, {runtime2}).".format(
        id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
        runtime1= jobs['execution_time'][a],
        runtime2= jobs['execution_time'][b]))

a = 0
b = 2
if abs(jobs['execution_time'][a] - jobs['execution_time'][b]) <= 100:
    print("OK. Jobs {id1} and {id2} should have different runtimes,"
          " but they have not ({runtime1}, {runtime2}).".format(
        id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
        runtime1= jobs['execution_time'][a],
        runtime2= jobs['execution_time'][b]))
    sys.exit(1)
else:
    print("OK. Jobs {id1} and {id2} have different runtimes "
          "({runtime1}, {runtime2}).".format(
        id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
        runtime1= jobs['execution_time'][a],
        runtime2= jobs['execution_time'][b]))

sys.exit(0)

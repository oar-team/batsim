#!/usr/bin/env python3
import json
import sys
from math import isclose
import pandas as pd

# Beginning of script
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))
assert(len(jobs) == 1)
job = jobs.iloc[0]


sched_input = json.load(open('{output_directory}/sched_input.json'.format(
    output_directory=sys.argv[1].replace('/out', ''))))
resource_fraction = sched_input['fraction_of_machines_to_use']
base_execution_time = 20.621958

if resource_fraction == 0.25:
    # Only one machine (resource_fraction == 0.25).
    # The machine is used 4 times more than usual.
    assert isclose(job['execution_time'], base_execution_time*4, abs_tol=1e-4)
elif resource_fraction == 0.5:
    # Only two machines (resource_fraction == 0.5).
    # The two machines are used 2 times more than usual.
    assert isclose(job['execution_time'], base_execution_time*2, abs_tol=1e-4)
elif resource_fraction == 0.75:
    # Only three machines (resource_fraction == 0.75).
    # One machine is used 2 times more than usual. The used is used as usual.
    assert isclose(job['execution_time'], base_execution_time*2, abs_tol=1e-4)
elif resource_fraction == 1.0:
    # Three machines (resource_fraction == 1).
    # All machines are used as usual.
    assert isclose(job['execution_time'], base_execution_time, abs_tol=1e-4)
else:
    # The resource_fraction is unexpected
    assert False

print('All the instances have been executed with expected makespan!')

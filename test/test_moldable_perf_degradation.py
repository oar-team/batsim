#!/usr/bin/env python3
'''Performance degradation tests when fewer resources are used for moldable jobs.

These tests run a single moldable profile in various situations and checks
that execution times are consistent.
'''
import json
import pandas as pd
import pytest
from math import isclose
from helper import *

# The fraction (in ]0,1]) of resources to use to execute the jobs.
ResourcesFractionMode = namedtuple('ResourcesFractionMode', ['name', 'value'])

def moldable_perf_degradation(platform, workload, algorithm, resource_fraction_to_use):
    test_name = f'moldable-perf-degrad-{resource_fraction_to_use.name}-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, schedconf_filename = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    schedconf_content = {
        "fraction_of_machines_to_use": resource_fraction_to_use.value,
        "custom_mapping": workload.name == 'compute1',
    }
    write_file(schedconf_filename, json.dumps(schedconf_content))

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}' --variant_options_filepath '{schedconf_filename}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    # Check execution time consistency in Batsim result file.
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    if len(jobs) != 1: raise Exception('Unexpected job count (not 1)')
    job = jobs.iloc[0]
    print(job)
    base_execution_time = 20.621958

    if workload.name == 'compute1':
        if resource_fraction_to_use.value == 0.25: # Only one machine.
            # The machine is used 4 times more than usual.
            expected_execution_time = base_execution_time * 4
        elif resource_fraction_to_use.value == 0.5: # Only two machines.
            # The two machines are used 2 times more than usual.
            expected_execution_time = base_execution_time * 2
        elif resource_fraction_to_use.value == 0.75: # Only three machines.
            # One machine is used 2 times more than usual. The other is used as usual.
            expected_execution_time = base_execution_time * 2
        elif resource_fraction_to_use.value == 1.0: # Four machines.
            # All machines are used as usual.
            expected_execution_time = base_execution_time
    elif workload.name == 'computetot1':
        expected_execution_time = base_execution_time / resource_fraction_to_use.value

    if not isclose(job['execution_time'], expected_execution_time, abs_tol=1e-4):
        raise Exception(f"Unexpected execution time (expected={expected_execution_time},got={job['execution_time']})")

@pytest.mark.parametrize("resource_fraction_to_use", [ResourcesFractionMode(f'{x:.2f}', x) for x in [0.25, 0.50, 0.75, 1.00]])

def test_moldable_perf_degradation(small_platform, moldable_perf_degradation_workload, filler_algorithm, resource_fraction_to_use):
    moldable_perf_degradation(small_platform, moldable_perf_degradation_workload, filler_algorithm, resource_fraction_to_use)

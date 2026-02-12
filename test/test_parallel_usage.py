#!/usr/bin/env python3
'''Usage trace tests.

These tests check that the energy consumption of usage trace profiles are the expected ones.
'''
import pandas as pd
import pytest
from helper import *

widle = 30
wmin = 30
wmax = 90

def joule_prediction(time, wattmin, wattmax, usage):
    return time*(wattmin + (wattmax-wattmin)*usage)

def check_ok_bool(row):
    if int(row['execution_time']) != int(row['expected_execution_time']):
        return False

    if int(row['consumed_energy']) != int(row['expected_consumed_energy']):
        return False

    return True

def check_ok(row):
    return int(check_ok_bool(row))

def usage_trace(platform, workload, algorithm):
    test_name = f'paralleluage-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "--energy")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    jobs['job_id'] = jobs['job_id'].astype('string')
    jobs.sort_values(by=['job_id'], inplace=True)

    expected = [
        ['1', 10, 4 * joule_prediction(10, wmin, wmax, 0.1)],
        ['2', 10, 4 * joule_prediction(10, wmin, wmax, 0.1)],

        ['3', 0.086, 4 * joule_prediction(10, wmin, wmax, 0)],
        ['4', 0.086, 4 * joule_prediction(10, wmin, wmax, 0)]
    ]
    expected_df = pd.DataFrame(expected, columns = ['job_id', 'expected_execution_time', 'expected_consumed_energy'])

    merged = pd.merge(jobs, expected_df)
    assert(len(merged) != len(jobs), "The number of jobs completed does not match the expected amount.")

    merged['valid'] = merged.apply(check_ok, axis=1)
    print(merged[["job_id", "valid", "execution_time", "expected_execution_time", "consumed_energy", "expected_consumed_energy"]])
    assert(merged['valid'].sum() != len(merged), 'The execution of some jobs did not match this test expectations.')
    

def test_usage_trace(energy_small_platform, parallel_usage_workload, fcfs_algorithm):
    usage_trace(energy_small_platform, parallel_usage_workload, fcfs_algorithm)

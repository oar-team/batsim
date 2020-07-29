#!/usr/bin/env python3
'''Walltime tests.

These tests checks that Batsim jobs do not exceed their walltimes.
'''
import pandas as pd
import pytest
from helper import *

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

def walltime(platform, workload, algorithm):
    test_name = f'walltime-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    # Analyze Batsim results to check whether execution times respect their walltimes.
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    jobs.sort_values(by=['job_id'], inplace=True)

    jobs['valid'] = jobs.apply(check_ok, axis=1)
    if jobs['valid'].sum() != len(jobs):
        print('Some jobs are invalid!')
        print(jobs[['job_id', 'requested_time', 'execution_time', 'success',
                    'valid']])
        raise Exception('The execution time of some jobs in invalid regarding their walltime.')
    else:
        print('All jobs are valid!')
        print(jobs[['job_id', 'requested_time', 'execution_time', 'success',
                    'valid']])

def test_walltime1(cluster_platform, small_workload, fcfs_algorithm):
    walltime(cluster_platform, small_workload, fcfs_algorithm)

def test_walltime2(cluster_platform, walltime_workload, fcfs_algorithm):
    walltime(cluster_platform, walltime_workload, fcfs_algorithm)

def test_walltime3(cluster_platform, walltime_smpi_workload, fcfs_algorithm):
    walltime(cluster_platform, walltime_smpi_workload, fcfs_algorithm)

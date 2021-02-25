#!/usr/bin/env python3
'''Walltime tests.

These tests run workFLOWs instead of workLOADs.
'''
import pandas as pd
import pytest
from helper import *

def tuto_stencil_test(platform, workload, algorithm):
    test_name = f'tuto_stencil-{algorithm.name}-{platform.name}-{workload.name}'
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

    # Analyze Batsim results to check whether execution times are consistent.
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    jobs.sort_values(by=['job_id'], inplace=True)

    assert(jobs.loc[0].execution_time < jobs.loc[1].execution_time)
    assert(jobs.loc[0].execution_time < jobs.loc[2].execution_time)

    assert(jobs.loc[0].execution_time < jobs.loc[3].execution_time)
    assert(jobs.loc[0].execution_time < jobs.loc[4].execution_time)
    assert(jobs.loc[3].execution_time < jobs.loc[2].execution_time)
    assert(jobs.loc[4].execution_time < jobs.loc[2].execution_time)

    assert(jobs.loc[4].execution_time < jobs.loc[5].execution_time)
    assert(jobs.loc[4].execution_time < jobs.loc[6].execution_time)
    assert(jobs.loc[4].execution_time < jobs.loc[7].execution_time)
    assert(jobs.loc[4].execution_time < jobs.loc[8].execution_time)

def test_tuto_stencil(cluster_pfs_platform, tuto_stencil_workload, fcfs_algorithm):
    tuto_stencil_test(cluster_pfs_platform, tuto_stencil_workload, fcfs_algorithm)

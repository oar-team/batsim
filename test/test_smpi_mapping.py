#!/usr/bin/env python3
'''SMPI mapping tests.

These tests checks whether rank mapping works as expected for SMPI jobs.
'''
import pandas as pd
import pytest
from helper import *

def smpi_mapping(platform, workload, algorithm):
    test_name = f'smpimapping-{algorithm.name}-{platform.name}-{workload.name}'
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

    # Analyze Batsim results to check whether the execution times are fine.
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    nb_err = 0

    # Jobs 1 and 2 should have similar execution times
    a = 0
    b = 1
    if abs(jobs['execution_time'][a] - jobs['execution_time'][b]) > 1:
        print("FAIL. Jobs {id1} and {id2} should have similar runtimes, but "
              "they have not ({runtime1}, {runtime2}).".format(
            id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
            runtime1= jobs['execution_time'][a],
            runtime2= jobs['execution_time'][b]))
        nb_err = nb_err + 1
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
        print("FAIL. Jobs {id1} and {id2} should have similar runtimes, but "
              "they have not ({runtime1}, {runtime2}).".format(
            id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
            runtime1= jobs['execution_time'][a],
            runtime2= jobs['execution_time'][b]))
        nb_err = nb_err + 1
    else:
        print("OK. Jobs {id1} and {id2} have similar runtimes "
              "({runtime1}, {runtime2}).".format(
            id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
            runtime1= jobs['execution_time'][a],
            runtime2= jobs['execution_time'][b]))

    a = 0
    b = 2
    if abs(jobs['execution_time'][a] - jobs['execution_time'][b]) <= 100:
        print("FAIL. Jobs {id1} and {id2} should have different runtimes,"
              " but they have not ({runtime1}, {runtime2}).".format(
            id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
            runtime1= jobs['execution_time'][a],
            runtime2= jobs['execution_time'][b]))
        nb_err = nb_err + 1
    else:
        print("OK. Jobs {id1} and {id2} have different runtimes "
              "({runtime1}, {runtime2}).".format(
            id1 = jobs['job_id'][a], id2= jobs['job_id'][b],
            runtime1= jobs['execution_time'][a],
            runtime2= jobs['execution_time'][b]))

    if nb_err > 0:
        raise Exception('Some jobs have an unexpected execution time')

def test_smpi_mapping(small_platform, smpi_mapping_workload, fcfs_algorithm):
    smpi_mapping(small_platform, smpi_mapping_workload, fcfs_algorithm)

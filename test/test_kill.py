#!/usr/bin/env python3
'''Kill tests.

These tests run many scenarios in which jobs are killed (KILL_JOB).
'''
import pandas as pd
import pytest
from helper import *

# Whether Redis should be used or not.
RedisMode = namedtuple('RedisMode', ['name', 'enabled'])

def kill_after10s(platform, workload, algorithm, redis_mode):
    test_name = f'kill-after10s-{algorithm.name}-{platform.name}-{workload.name}-{redis_mode.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batparams = "--forward-profiles-on-submission"
    if redis_mode.enabled == True: batparams = batparams + " --enable-redis"

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, batparams)
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    # Analyze Batsim results
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)

    epsilon = 0.1
    max_runtime = 10 + epsilon

    jobs_too_long = jobs.loc[jobs['execution_time'] > max_runtime]

    if jobs_too_long.empty:
        print(f'All jobs have been shorter than {max_runtime} s')
    else:
        print(f'Some jobs were longer than {max_runtime} s')
        print(jobs_too_long)
        raise Exception('Some were longer than f{max_runtime} s')

def test_kill_after10s(small_platform, small_workload, killer_algorithm, redis_mode):
    if small_workload.name == 'delaysequences':
        pytest.xfail("something seems wrong with sequences")
    kill_after10s(small_platform, small_workload, killer_algorithm, redis_mode)

#!/usr/bin/env python3
'''Kill tests.

These tests run many scenarios in which jobs are killed (KILL_JOB).
'''
import json
import pandas as pd
import pytest
from helper import *

# The number of kills to do for each job.
KillCountMode = namedtuple('KillCountMode', ['name', 'kill_count_per_job'])

# The number of seconds between a job execution and its kill.
KillDelayMode = namedtuple('KillDelayMode', ['name', 'kill_delay'])

def kill_after_delay(platform, workload, algorithm, redis_mode, nb_kills_per_job, delay_before_kill):
    test_name = f'kill-{delay_before_kill.name}-{nb_kills_per_job.name}-{algorithm.name}-{platform.name}-{workload.name}-{redis_mode.name}'
    output_dir, robin_filename, schedconf_filename = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batparams = "--forward-profiles-on-submission"
    if redis_mode.enabled == True: batparams = batparams + " --enable-redis"
    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, batparams)

    schedconf_content = {
        "delay_before_kill": delay_before_kill.kill_delay,
        "nb_kills_per_job": nb_kills_per_job.kill_count_per_job,
    }
    write_file(schedconf_filename, json.dumps(schedconf_content))

    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}' --variant_options_filepath '{schedconf_filename}'",
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
    max_runtime = delay_before_kill.kill_delay + epsilon

    jobs_too_long = jobs.loc[jobs['execution_time'] > max_runtime]

    if jobs_too_long.empty:
        print(f'All jobs have been shorter than {max_runtime} s')
    else:
        print(f'Some jobs were longer than {max_runtime} s')
        print(jobs_too_long)
        raise Exception('Some were longer than f{max_runtime} s')

@pytest.mark.parametrize("nb_kills_per_job", [KillCountMode(f'{n}kills', n) for n in [1,2]])
@pytest.mark.parametrize("delay_before_kill", [KillDelayMode(f'after{n}', n) for n in [5,10,15]])
def test_kill_after_delay(small_platform, small_workload, killer_algorithm, redis_mode, nb_kills_per_job, delay_before_kill):
    if small_workload.name == 'delaysequences':
        pytest.skip("something seems wrong with sequences")
    kill_after_delay(small_platform, small_workload, killer_algorithm, redis_mode, nb_kills_per_job, delay_before_kill)

@pytest.mark.parametrize("nb_kills_per_job", [KillCountMode(f'{n}kills', n) for n in [1,2]])
@pytest.mark.parametrize("delay_before_kill", [KillDelayMode(f'after{n}', n) for n in [5,10,15]])
@pytest.mark.xfail(reason="https://gitlab.inria.fr/batsim/batsim/issues/108")
def test_kill_after_delay_sequences(small_platform, delaysequences_workload, killer_algorithm, redis_mode, nb_kills_per_job, delay_before_kill):
    kill_after_delay(small_platform, delaysequences_workload, killer_algorithm, redis_mode, nb_kills_per_job, delay_before_kill)

@pytest.mark.parametrize("nb_kills_per_job", [KillCountMode(f'{n}kills', n) for n in [1,2]])
@pytest.mark.parametrize("delay_before_kill", [KillDelayMode(f'after{n}', n) for n in [0]])
@pytest.mark.xfail(reason="https://gitlab.inria.fr/batsim/batsim/issues/37")
def test_kill_after_delay0(small_platform, small_workload, killer_algorithm, redis_mode, nb_kills_per_job, delay_before_kill):
    if small_workload.name == 'delaysequences':
        pytest.xfail("something seems wrong with sequences")
    kill_after_delay(small_platform, small_workload, killer_algorithm, redis_mode, nb_kills_per_job, delay_before_kill)

#!/usr/bin/env python3
'''Usage trace tests.

These tests check that the energy consumption of usage trace profiles are the expected ones.
'''
import inspect
import pandas as pd
import pytest
from helper import *

MOD_NAME = __name__.replace('test_', '', 1)

fast_speed = 100
fast_widle = 50
fast_wmin = 100
fast_wmax = 200

slow_speed = 50
slow_widle = 50
slow_wmin = 90
slow_wmax = 150

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

def estimate_job_from_real_trace():
    traces = [
        (0, 0.86, 1186),
        (0, 0.64, 469),
        (0, 0.79, 456),
        (0, 0.84, 4643),
        (0, 0.85, 4659),
        (0, 0.9, 1000),
        (0, 0.83, 3614),
        (0, 0.84, 4643),
        (0, 0.9, 933),
        (0, 0.83, 3759),
        (0, 0.89, 1011),
        (0, 0.83, 3614),
        (0, 0.84, 4571),
        (0, 0.91, 923),
        (0, 0.84, 3643),
        (0, 0.89, 1079),
        (0, 0.83, 3614),
        (0, 0.85, 4588),
        (0, 0.85, 4588),
        (0, 0.91, 989),
        (0, 0.84, 3643),
        (0, 0.85, 4659),
        (0, 0.84, 4643),
        (0, 0.88, 3068),
        (0, 0.79, 1519),
        (0, 0.89, 674),

        (1, 0.98, 1041),
        (1, 0.73, 411),
        (1, 0.88, 409),
        (1, 0.98, 2755),
        (1, 0.88, 1364),
        (1, 1.0, 1020),
        (1, 0.94, 3383),
        (1, 0.99, 2424),
        (1, 0.88, 1432),
        (1, 0.98, 2694),
        (1, 0.89, 1416),
        (1, 1, 840),
        (1, 0.97, 2041),
        (1, 0.89, 1281),
        (1, 0.99, 2667),
        (1, 0.89, 1416),
        (1, 0.99, 2667),
        (1, 0.89, 1348),
        (1, 1, 840),
        (1, 0.98, 1898),
        (1, 0.89, 1348),
        (1, 0.98, 2816),
        (1, 0.88, 1364),
        (1, 0.97, 18990),
        (1, 0.89, 1348),
        (1, 0.99, 2727),
        (1, 0.89, 1348),
        (1, 1.0, 600),

        (2, 0.84, 1429),
        (2, 0.64, 469),
        (2, 0.86, 3837),
        (2, 0.75, 960),
        (2, 0.9, 667),
        (2, 0.86, 3070),
        (2, 0.76, 947),
        (2, 0.86, 3698),
        (2, 0.76, 947),
        (2, 0.86, 3767),
        (2, 0.76, 868),
        (2, 0.86, 3837),
        (2, 0.75, 880),
        (2, 0.86, 3698),
        (2, 0.75, 960),
        (2, 0.86, 3628),
        (2, 0.75, 960),
        (2, 0.86, 3698),
        (2, 0.75, 960),
        (2, 0.86, 3767),
        (2, 0.75, 960),
        (2, 0.85, 3741),
        (2, 0.75, 960),
        (2, 0.86, 3698),
        (2, 0.76, 947),
        (2, 0.87, 3724),
        (2, 0.75, 960),
        (2, 0.87, 3655),
        (2, 0.75, 960),
        (2, 0.91, 593),
        (2, 0.85, 3176),
        (2, 0.75, 960),
        (2, 0.85, 3741),
        (2, 0.76, 947),
        (2, 0.88, 682),

        (3, 0.87, 1379),
        (3, 0.66, 455),
        (3, 0.89, 3708),
        (3, 0.77, 935),
        (3, 0.9, 3600),
        (3, 0.78, 923),
        (3, 0.89, 3573),
        (3, 0.78, 923),
        (3, 0.88, 3750),
        (3, 0.77, 779),
        (3, 0.89, 3708),
        (3, 0.78, 846),
        (3, 0.89, 3573),
        (3, 0.77, 935),
        (3, 0.89, 3506),
        (3, 0.77, 935),
        (3, 0.9, 3533),
        (3, 0.77, 935),
        (3, 0.89, 3640),
        (3, 0.77, 935),
        (3, 0.89, 3573),
        (3, 0.77, 935),
        (3, 0.91, 3495),
        (3, 0.78, 923),
        (3, 0.9, 3600),
        (3, 0.78, 923),
        (3, 0.9, 3533),
        (3, 0.78, 923),
        (3, 0.9, 3600),
        (3, 0.78, 923),
        (3, 0.89, 3573),
        (3, 0.77, 935),
        (3, 0.91, 659),
    ]
    traces_df = pd.DataFrame(traces, columns = ['machine_id', 'usage', 'flops'])

    # job allocation (rank->machine_type)
    machines = [
        (0, 'fast'),
        (1, 'fast'),
        (2, 'slow'),
        (3, 'slow'),
    ]
    machines_df = pd.DataFrame(machines, columns = ['machine_id', 'machine_type'])

    # parameters of each type of machine
    machine_types = [
        ('fast', fast_speed, fast_widle, fast_wmin, fast_wmax),
        ('slow', slow_speed, slow_widle, slow_wmin, slow_wmax),
    ]
    machine_types_df = pd.DataFrame(machine_types, columns = ['machine_type', 'speed', 'widle', 'wmin', 'wmax'])

    df = pd.merge(traces_df, pd.merge(machines_df, machine_types_df))
    df['duration'] = df['flops'] / df['speed']
    df['w'] = df['wmin'] + (df['wmax'] - df['wmin']) * df['usage']
    df['joules'] = df['w'] * df['duration']

    idles = df.groupby(['machine_id'])['duration'].agg('sum').to_frame()
    idles.reset_index(level=0, inplace=True)
    idles['job_duration'] = idles['duration'].max()
    idles['idle_time'] = idles['job_duration'] - idles['duration']
    idles = pd.merge(idles, pd.merge(machines_df, machine_types_df))
    idles['joules'] = idles['widle'] * idles['idle_time']

    job_execution_time = idles['duration'].max()
    job_joules = idles['joules'].sum() + df['joules'].sum()
    return ['60', job_execution_time, job_joules]

def test_check_energy_consumed(test_root_dir):
    platform = 'small_platform_replay_usage'
    workload = 'test_usage_trace'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    wload_name = workload.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{wload_name}'

    timeout = int(os.getenv('TEST_INSTANCE_TIMEOUT', '5')) * 3
    batcmd, outdir = prepare_instance(instance_name, test_root_dir, platform, 'fcfs', workload, batsim_extra_args=['--energy-host'])
    p = run_batsim(batcmd, outdir, timeout=timeout)
    assert p.returncode == 0

    # analyze Batsim results to check their energy consumption is the expected one.
    batjobs_filename = f'{outdir}/batout/jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    jobs['job_id'] = jobs['job_id'].astype('string')
    jobs.sort_values(by=['job_id'], inplace=True)

    expected = [
        ['0', 10, 2*joule_prediction(10, fast_widle, fast_widle, 0.0)],

        ['10', 10, 2*joule_prediction(10, fast_wmin, fast_wmax, 1.0)],
        ['11', 20, 2*joule_prediction(20, slow_wmin, slow_wmax, 1.0)],

        ['20', 10, 2*joule_prediction(10, fast_wmin, fast_wmax, 0.5)],
        ['21', 20, 2*joule_prediction(20, slow_wmin, slow_wmax, 0.5)],

        ['30', 20, 2*(joule_prediction(10, fast_wmin, fast_wmax, 1.0)+joule_prediction(10, fast_wmin, fast_wmax, 0.1))],
        ['31', 40, 2*(joule_prediction(20, slow_wmin, slow_wmax, 1.0)+joule_prediction(20, slow_wmin, slow_wmax, 0.1))],

        ['40', 10, joule_prediction(10, fast_wmin, fast_wmax, 0.2)+joule_prediction(10, fast_wmin, fast_wmax, 0.6)],
        ['41', 20, joule_prediction(20, slow_wmin, slow_wmax, 0.2)+joule_prediction(20, slow_wmin, slow_wmax, 0.6)],

        ['50', 20, joule_prediction(10, fast_wmin, fast_wmax,  0.1)+joule_prediction(10, fast_widle, fast_widle, 0.0) +
                   joule_prediction(10, fast_wmin, fast_wmax, 0.01)+joule_prediction(10, fast_wmin, fast_wmax, 0.97)],
        ['51', 40, joule_prediction(20, slow_wmin, slow_wmax,  0.1)+joule_prediction(20, slow_widle, slow_widle, 0.0) +
                   joule_prediction(20, slow_wmin, slow_wmax, 0.01)+joule_prediction(20, slow_wmin, slow_wmax, 0.97)],

        estimate_job_from_real_trace()
    ]
    expected_df = pd.DataFrame(expected, columns = ['job_id', 'expected_execution_time', 'expected_consumed_energy'])

    merged = pd.merge(jobs, expected_df)
    if len(merged) != len(jobs):
        raise Exception('There are {} jobs in the workload but only {} jobs are known by the test'.format(len(jobs), len(merged)))

    merged['valid'] = merged.apply(check_ok, axis=1)
    if merged['valid'].sum() != len(merged):
        print('Some jobs are invalid!')
        print(merged[['job_id', 'valid', 'execution_time', 'expected_execution_time', 'consumed_energy', 'expected_consumed_energy']])
        raise Exception('The execution of some jobs did not match this test expectations.')
    else:
        print('All jobs are valid!')
        print(merged[['job_id', 'valid', 'execution_time', 'expected_execution_time', 'consumed_energy', 'expected_consumed_energy']])


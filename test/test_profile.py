#!/usr/bin/env python3
'''Profile tests.

These tests run batsim with workloads that consist of specific profile types.
'''
import inspect
import json
import os
import subprocess
import pandas as pd
import pytest

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[
    ('test_smpi_compute_only', 1),
    ('test_smpi_compute_only_2_jobs', 1),
    ('test_smpi_mixed_comp_comm', 1),
    ('test_smpi_collectives', 3),
])
def smpi_workload_timeoutscale(request):
    return request.param

def test_delay(test_root_dir):
    platform = 'cluster512'
    workload = 'test_delays'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1', workload)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_replay_smpi(test_root_dir, smpi_workload_timeoutscale):
    platform = 'small_platform'
    smpi_workload, timeout_scale = smpi_workload_timeoutscale
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    wload_name = smpi_workload.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{wload_name}'

    timeout = int(os.getenv('TEST_INSTANCE_TIMEOUT', '5')) * timeout_scale
    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1', smpi_workload)
    p = run_batsim(batcmd, outdir, timeout=timeout)
    assert p.returncode == 0

def compute_job_expected_state(row):
    if row['requested_time'] < row['profile_expected_execution_time']:
        return 'COMPLETED_WALLTIME_REACHED'
    else:
        return 'COMPLETED_SUCCESSFULLY'

def compute_job_state_time_ok(row):
    return row['job_expected_state'] == row['final_state'] and row['job_expected_execution_time'] == row['execution_time']

def run_instance_check_job_duration_and_state(test_root_dir, platform, workload, instance_name, edc):
    batcmd, outdir, workload_file = prepare_instance(instance_name, test_root_dir, platform, edc, workload)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

    batjobs_filename = f'{outdir}/batout/jobs.csv'
    jobs = pd.read_csv(batjobs_filename)

    f = open(workload_file)
    batw = json.load(f)
    profile_names = list()
    expected_execution_times = list()

    for profile_name, profile in batw['profiles'].items():
        expected_execution_time = profile['expected_execution_time']
        profile_names.append(profile_name)
        expected_execution_times.append(expected_execution_time)

    expected_df = pd.DataFrame({'profile': profile_names, 'profile_expected_execution_time': expected_execution_times})

    df = jobs.merge(expected_df, on='profile', how='inner')
    df['job_expected_execution_time'] = df[['profile_expected_execution_time', 'requested_time']].min(axis=1)
    df['job_expected_state'] = df.apply(compute_job_expected_state, axis=1)
    df['all_good'] = df.apply(compute_job_state_time_ok, axis=1)

    with pd.option_context('display.max_rows', None, 'display.max_columns', None):
        if not df['all_good'].all():
            unexpected_df = df[df['all_good'] == False]
            printable_df = unexpected_df[['job_id', 'profile', 'requested_time', 'execution_time', 'job_expected_execution_time', 'final_state', 'job_expected_state']]
            print('Some jobs have an unexpected execution time or final state')
            print(printable_df)
            raise ValueError('Some jobs have an unexpected execution time or final state')
        else:
            print('All jobs are valid!')
            printable_df = df[['job_id', 'profile', 'requested_time', 'execution_time', 'job_expected_execution_time', 'final_state', 'job_expected_state']]
            print(printable_df)

def test_ptask(test_root_dir):
    platform = 'cluster512'
    workload = 'test_ptasks'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'
    edc = 'exec1by1'

    run_instance_check_job_duration_and_state(test_root_dir, platform, workload, instance_name, edc)

def test_ptask_homogeneous(test_root_dir):
    platform = 'cluster512'
    workload = 'test_homo_ptasks'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'
    edc = 'exec1by1'

    run_instance_check_job_duration_and_state(test_root_dir, platform, workload, instance_name, edc)

def test_sequential_composition_delay(test_root_dir):
    platform = 'cluster512'
    workload = 'test_compo_sequence_delay'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'
    edc = 'exec1by1'

    run_instance_check_job_duration_and_state(test_root_dir, platform, workload, instance_name, edc)

def test_sequential_composition_ptaskcomp(test_root_dir):
    platform = 'cluster512'
    workload = 'test_compo_sequence_ptaskcomp'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'
    edc = 'exec1by1'

    run_instance_check_job_duration_and_state(test_root_dir, platform, workload, instance_name, edc)

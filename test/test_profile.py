#!/usr/bin/env python3
'''Profile tests.

These tests run batsim with workloads that consist of specific profile types.
'''
import inspect
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim, run_instance_check_job_duration_and_state

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[
    ('test_smpi_compute_only', 1),
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
    platform = 'cluster512'
    smpi_workload, timeout_scale = smpi_workload_timeoutscale
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    wload_name = smpi_workload.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{wload_name}'
    timeout = int(os.getenv('TEST_INSTANCE_TIMEOUT', '5')) * timeout_scale
    edc = 'exec1by1'

    run_instance_check_job_duration_and_state(test_root_dir, platform, smpi_workload, instance_name, edc, timeout)

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

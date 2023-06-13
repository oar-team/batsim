#!/usr/bin/env python3
'''Profile tests.

These tests run batsim with workloads that consist of specific profile types.
'''
import inspect
import os
import subprocess
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

    batcmd, outdir = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1', workload)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_smpi(test_root_dir, smpi_workload_timeoutscale):
    platform = 'small_platform'
    smpi_workload, timeout_scale = smpi_workload_timeoutscale
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    wload_name = smpi_workload.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{wload_name}'

    timeout = int(os.getenv('TEST_INSTANCE_TIMEOUT', '5')) * timeout_scale
    batcmd, outdir = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1', smpi_workload)
    p = run_batsim(batcmd, outdir, timeout=timeout)
    assert p.returncode == 0

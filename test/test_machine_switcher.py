#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with various machines switch on/off scenarios.
'''
import inspect
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[
    ("run_ok", 0),
    ("turn_on_fail", -6),
    ("turn_off_fail", -6),
])
def parameters(request):
    return request.param

def test_machine_switcher(test_root_dir):
    edc_init_args = {"option": "run_ok"}

    platform = 'energy_platform_homogeneous'
    workload = 'test_one_delay_job'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-run_ok'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'machine-switcher', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)

    assert p.returncode == 0

def test_machine_switcher_turn_off_fail(test_root_dir):
    edc_init_args = {"option": "turn_off_fail"}

    platform = 'energy_platform_homogeneous'
    workload = 'test_one_delay_job'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-turn_off_fail'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'machine-switcher', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)

    assert p.returncode != 0

    with open(f'{outdir}/batsim.stderr', 'r') as errfile:
        l = errfile.read()
        assert 'Asked to turn OFF a host already in a sleep pstate' in l, f'batsim was expected to crash during an assert'

def test_machine_switcher_turn_on_fail(test_root_dir):
    edc_init_args = {"option": "turn_on_fail"}

    platform = 'energy_platform_homogeneous'
    workload = 'test_one_delay_job'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-turn_on_fail'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'machine-switcher', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)

    assert p.returncode != 0

    with open(f'{outdir}/batsim.stderr', 'r') as errfile:
        l = errfile.read()
        assert 'Asked to turn ON a host already in a computing pstate' in l, f'batsim was expected to crash during an assert'


def test_machine_switcher_job_running_fail(test_root_dir):
    edc_init_args = {"option": "job_running_fail"}

    platform = 'energy_platform_homogeneous'
    workload = 'test_one_delay_job'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-job_running_fail'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'machine-switcher', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)

    assert p.returncode != 0

    with open(f'{outdir}/batsim.stderr', 'r') as errfile:
        l = errfile.read()
        assert 'jobs are running on machine' in l, f'batsim was expected to crash during an assert'


def test_machine_switcher_job_execute_fail(test_root_dir):
    edc_init_args = {"option": "job_execute_fail"}

    platform = 'energy_platform_homogeneous'
    workload = 'test_one_delay_job'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-job_execute_fail'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'machine-switcher', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)

    assert p.returncode != 0

    with open(f'{outdir}/batsim.stderr', 'r') as errfile:
        l = errfile.read()
        assert 'cannot compute jobs now (the machine is not computing nor idle' in l, f'batsim was expected to crash during an assert'


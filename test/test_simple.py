#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with most basic features (execute jobs, reject jobs).
'''
import inspect
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module")
def platform():
    return 'small_platform'

@pytest.fixture(scope="module")
def workload():
    return 'test_delays'

@pytest.fixture(scope="module", params=[False, True])
def use_json(request):
    return request.param

def test_rejecter(test_root_dir, platform, workload):
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{workload}'

    batcmd, outdir = prepare_instance(instance_name, test_root_dir, platform, 'rejecter', workload)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_1by1(test_root_dir, platform, workload):
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{workload}'

    batcmd, outdir = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1', workload)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_fcfs(test_root_dir, platform, workload, use_json):
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{workload}-' + str(int(use_json))

    batcmd, outdir = prepare_instance(instance_name, test_root_dir, platform, 'fcfs', workload, use_json=use_json)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

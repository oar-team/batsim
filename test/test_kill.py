#!/usr/bin/env python3
'''Kill tests.

These tests kill jobs in various ways to make sure that Batsim supports it.
'''
import inspect
import json
import os
import subprocess
import pytest
import pandas as pd

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[False, True])
def use_json(request):
    return request.param

@pytest.fixture(scope="module", params=[0.0, 1e-10, 1.0, 100.0])
def kill_delay(request):
    return request.param

@pytest.fixture(scope="module", params=['test_delays', 'test_ptasks', 'test_homo_ptasks', 'test_compo_sequence_delay', 'test_compo_sequence_ptaskcomp'])
def workload(request):
    return request.param

def test_afterd(test_root_dir, kill_delay, workload):
    platform = 'cluster512'
    workload_name = workload.replace('test_', '', 1)
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{kill_delay}-{workload_name}'
    edc_init_args = {
        'kill_delay': kill_delay,
    }

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'killer', workload, edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True))
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

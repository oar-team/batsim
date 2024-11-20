#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with most basic features (execute jobs, reject jobs).
'''
import inspect
import json
import os
import subprocess
import pytest
import pandas as pd
import random

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=['wload', 'deadline'])
def behavior(request):
    return request.param

@pytest.fixture(scope="module", params=[0.0, 0.5])
def inter_stop_probe_delay(request):
    return request.param

def test_energy(test_root_dir, behavior, inter_stop_probe_delay):
    platform = 'cluster_energy_128'
    workload = 'test_homo_ptasks'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{behavior}-{inter_stop_probe_delay}'

    edc_init_args = {
        'behavior': behavior,
        'inter_stop_probe_delay': inter_stop_probe_delay,
    }

    batargs = ["--energy-host"]
    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'probe-energy', workload, edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True), batsim_extra_args=batargs)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

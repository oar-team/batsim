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

@pytest.fixture(scope="module", params=[False, True])
def issue_all_calls_at_start(request):
    return request.param

@pytest.fixture(scope="module", params=['s', 'ms'])
def time_unit(request):
    return request.param

@pytest.fixture(scope="module", params=[False, True])
def is_infinite(request):
    return request.param

@pytest.fixture(scope="module", params=[i for i in range(10)])
def seed(request):
    return request.param

def test_oneshot(test_root_dir, issue_all_calls_at_start, time_unit):
    platform = 'small_platform'
    workload = 'test_delays'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-' + str(int(issue_all_calls_at_start)) + '-' + time_unit

    edc_init_args = {
        'issue_all_calls_at_start': issue_all_calls_at_start,
        'time_unit': time_unit,
        'calls': [10, 100, 1000, 10000],
    }

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'call-later-oneshot', workload, edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True))
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_periodic_start0(test_root_dir, time_unit, is_infinite):
    platform = 'small_platform'
    workload = 'test_delays'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{time_unit}-' + str(int(is_infinite))

    edc_init_args = {
        'is_infinite': is_infinite,
        'time_unit': time_unit,
        'calls': [
            {'init': 0, 'period': 10, 'nb': 1093},
            {'init': 0, 'period': 100, 'nb': 179},
            {'init': 0, 'period': 1000, 'nb': 13},
            {'init': 0, 'period': 10000, 'nb': 3},
        ],
    }

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'call-later-periodic', workload, edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True))
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_periodic_rand(test_root_dir, seed, time_unit, is_infinite):
    platform = 'small_platform'
    workload = 'test_delays'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{seed}-{time_unit}-' + str(int(is_infinite))

    random.seed(seed)
    nb_periodic_calls = random.randint(2, 5)
    base_period = random.randint(5, 16)
    calls = list()
    for _ in range(nb_periodic_calls):
        calls.append({
            'init': random.randint(0, 100),
            'period': base_period ** random.randint(1, 4),
            'nb': random.randint(1, 1000),
        })

    edc_init_args = {
        'is_infinite': is_infinite,
        'time_unit': time_unit,
        'calls': calls,
    }

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'call-later-periodic', workload, edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True))
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

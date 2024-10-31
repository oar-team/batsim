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

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[False, True])
def issue_all_calls_at_start(request):
    return request.param

def test_call_me_later_oneshot(test_root_dir, issue_all_calls_at_start):
    platform = 'small_platform'
    workload = 'test_delays'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-' + str(int(issue_all_calls_at_start))

    edc_init_args = {
        'issue_all_calls_at_start': issue_all_calls_at_start,
        'calls': [10, 100, 1000, 10000],
    }

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'call-later-oneshot', workload, edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True))
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

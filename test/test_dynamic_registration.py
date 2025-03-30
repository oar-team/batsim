#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with various dynamic registration of job/profile scenarios.
'''
import inspect
import json
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[
    ("ack_jobs_ok", 0),
    ("noack_jobs_ok", 0),
    ("identical_job_names_fail", -6),
    ("identical_profile_names_fail", -6),
    ("profile_reuse_fail", -6),
    ("profile_reuse_ok", 0),
])
def parameters(request):
    return request.param

def test_dyn_register(test_root_dir, parameters):
    init_option_str, expected_ret_code = parameters

    platform = 'small_platform'
    workload = 'test_one_delay_job'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{init_option_str}'

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'dyn-register', workload, edc_init_content=init_option_str)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == expected_ret_code

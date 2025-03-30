#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with an EDC that submit and execute random (profile, job) on random dvfs states on a known platform, and checks their runtimes.
'''
import inspect
import json
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim, EXTERNAL_EVENTS_DIR

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[81,169,361])
def random_seed(request):
    return request.param

def test_dvfs(test_root_dir, random_seed):
    platform = 'small_platform_dvfs'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{random_seed}'

    edc_init_args = {
        "random_seed": random_seed,
        "nb_jobs_to_submit": 10,
    }

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1-dvfs-dyn', edc_init_content=json.dumps(edc_init_args, allow_nan=False, sort_keys=True))
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

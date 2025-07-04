#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with various custom mappings for job execution.
'''
import inspect
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

def test_custom_mapping(test_root_dir):
    edc_init_args = {"do_fail_rigid": False}

    platform = 'small_platform'
    workload = 'test_various_profile_types'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1-custom-mapping', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

def test_custom_mapping_fail(test_root_dir):
    edc_init_args = {"do_fail_rigid": True}

    platform = 'small_platform'
    workload = 'test_various_profile_types'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'exec1by1-custom-mapping', workload, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)
    assert p.returncode != 0, f"batsim return 0 while a bad custom executor mapping was given"

    with open(f'{outdir}/batsim.stderr', 'r') as errfile:
        l = errfile.read()
        assert "inconsistent placement for job='w0!simple': profile 'w0!simple' is rigid (profile_type='PTASK') but user-given custom mapping has size=2, which is not equal to nb_res=4 requested by the job" in l, f"batsim did not give a message on stderr about a bad custom executor mapping on a rigid profile"

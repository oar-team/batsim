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

def test_dyn_register(test_root_dir):
    platform = 'small_platfor'
    workload = 'test_one_delay'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}'

    batcmd, outdir, _ = prepare_instance(instance_name, test_root_dir, platform, 'dyn_register', workload)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

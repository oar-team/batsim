#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with most basic features (execute jobs, reject jobs).
'''
import inspect
import json
import os
import subprocess
import pytest
import shlex
import time
import pandas as pd

from helper import prepare_instance, run_batsim

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[False, True])
def use_json(request):
    return request.param

def run_two_process_sim(batcmd, edccmd, output_dir, timeout=None):
    bat_stdout_file = f'{output_dir}/batsim.stdout'
    bat_stderr_file = f'{output_dir}/batsim.stdout'
    edc_stdout_file = f'{output_dir}/edc.stdout'
    edc_stderr_file = f'{output_dir}/edc.stdout'

    with open(bat_stdout_file, "wb") as bat_out, open(bat_stderr_file, "wb") as bat_err, \
         open(edc_stdout_file, "wb") as edc_out, open(edc_stderr_file, "wb") as edc_err:

        process_bat = subprocess.Popen(batcmd, stdout=bat_out, stderr=bat_err)
        process_edc = subprocess.Popen(edccmd, stdout=edc_out, stderr=edc_err)

        start_time = time.time()

        try:
            remaining = None if timeout is None else timeout - (time.time() - start_time)
            process_bat.wait(timeout=remaining)
            remaining = None if timeout is None else timeout - (time.time() - start_time)
            process_edc.wait(timeout=remaining)
        except subprocess.TimeoutExpired:
            print(f'Timeout ({timeout} s) expired while waiting for batsim+EDC process termination!')
            if process_bat.poll() is None:
                process_bat.terminate()
            if process_edc.poll() is None:
                process_edc.terminate()
            try:
                process_bat.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process_bat.kill()
            try:
                process_edc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process_edc.kill()
        return process_bat, process_edc

def test_fcfs(test_root_dir, use_json):
    platform = 'small_platform'
    workload = 'test_delays'
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-' + str(int(use_json))
    timeout = 5

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'fcfs', workload, use_json=use_json, edc_is_lib=False)
    socket_path = f'{os.path.abspath(outdir)}/sock'
    edccmd = [
        'process-edc',
        '--socket-endpoint', f'ipc://{socket_path}'
    ]
    edccmd_filename = f'{outdir}/edc.sh'
    descriptor = os.open(path=edccmd_filename, flags=os.O_WRONLY|os.O_CREAT|os.O_TRUNC, mode=0o700)
    with open(descriptor, 'w') as f:
        f.write(shlex.join(edccmd) + '\n')

    batp, edcp = run_two_process_sim(batcmd, edccmd, outdir, timeout)
    try:
        os.remove(socket_path)
    except Exception:
        pass
    assert batp.returncode == 0
    assert edcp.returncode == 0

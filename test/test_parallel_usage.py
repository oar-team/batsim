#!/usr/bin/env python3
'''Usage trace tests.

These tests check that the energy consumption of usage trace profiles are the expected ones.
'''
import pandas as pd
import pytest
from helper import *

widle = 30
wmin = 30
wmax = 90

def joule_prediction(time, wattmin, wattmax, usage):
    return time*(wattmin + (wattmax-wattmin)*usage)

def check_ok_bool(row):
    if int(row['execution_time']) != int(row['expected_execution_time']):
        return False

    if int(row['consumed_energy']) != int(row['expected_consumed_energy']):
        return False

    return True

def check_ok(row):
    return int(check_ok_bool(row))

def usage_trace(platform, workload, algorithm):
    test_name = f'paralleluage-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "--energy")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')


def test_usage_trace(energy_small_platform, parallel_usage_workload, fcfs_algorithm):
    usage_trace(energy_small_platform, parallel_usage_workload, fcfs_algorithm)

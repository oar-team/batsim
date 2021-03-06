#!/usr/bin/env python3
'''Walltime tests.

These tests run workFLOWs instead of workLOADs.
'''
import pandas as pd
import pytest
from helper import *

def workflow_test(platform, workload, algorithm):
    test_name = f'workflow-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "", is_workload=False)
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

def test_workflow1(cluster_platform, workflow, fcfs_algorithm):
    workflow_test(cluster_platform, workflow, fcfs_algorithm)

#!/usr/bin/env python3
'''No-scheduler tests.

These tests run batsim without external scheduler.
'''
from helper import *

def nosched(platform, workload):
    test_name = f'nosched-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "--no-sched")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd="",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

def test_nosched1(platform, small_workload):
    nosched(platform, small_workload)

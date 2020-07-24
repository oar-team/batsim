#!/usr/bin/env python3
'''Resource sharing tests.

These tests run batsim in various resource sharing modes.
'''
from helper import *

def resourcesharing(platform, workload, algorithm):
    test_name = f'resourcesharing-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "--enable-compute-sharing")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    # TODO: make sure the resources were shared in the batsim result files

def test_resourcesharing(cluster_platform, long_workload, random_algorithm):
    resourcesharing(cluster_platform, long_workload, random_algorithm)

# TODO: use a non-random algorithm that uses the same resources.
# TODO: test storage sharing

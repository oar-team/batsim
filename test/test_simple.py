#!/usr/bin/env python3
from helper import *

def test_basic(platform, one_job_workload, basic_algorithm):
    test_name = f'basic-{basic_algorithm.name}-{platform.name}-{one_job_workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if basic_algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, one_job_workload.filename, output_dir, "")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{basic_algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

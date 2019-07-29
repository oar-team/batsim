#!/usr/bin/env python3
''' Test for host and zone properties on XML platform. '''

from helper import *
import pytest

def simple_platform_properties(platform, workload, algorithm):
	test_name = f'simple-platform-properties-{algorithm.name}-{platform.name}-{workload.name}'
	output_dir, robin_filename, _ = init_instance(test_name)

	if algorithm.sched_implem != 'pybatsim': raise Exception('This test only supports pybatsim for now')

	batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "")
	instance = RobinInstance(output_dir=output_dir,
		batcmd=batcmd,
		schedcmd=f"pybatsim {algorithm.sched_algo_name}",
		simulation_timeout=30, ready_timeout=5,
		success_timeout=10, failure_timeout=0
	)

	instance.to_file(robin_filename)
	ret = run_robin(robin_filename)
	if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

def test_simple_platform_properties(properties_platform, mixed_workload, pybatsim_filler_algorithm):
	simple_platform_properties(properties_platform, mixed_workload, pybatsim_filler_algorithm)

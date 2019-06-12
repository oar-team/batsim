#!/usr/bin/env python3
''' External events tests.

These tests run batsim with external events inputs.
'''
from helper import *
import pytest

def externalevents(platform, workload, algorithm, events):
    test_name = f'external-events-{algorithm.name}-{platform.name}-{workload.name}-{events.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'pybatsim': raise Exception('This test only supports pybatsim for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, f'--events {events.filename}')
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"pybatsim {algorithm.sched_algo_name}",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')



def genericevents(platform, workload, algorithm, events, option_forward_generic):
    test_name = f'generic-events-{algorithm.name}-{platform.name}-{workload.name}-{events.name}-forward-{option_forward_generic}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'pybatsim': raise Exception('This test only supports pybatsim for now')

    option_string = f'--events {events.filename}'
    if option_forward_generic:
        option_string+= " --forward-unknown-events"

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, option_string)
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"pybatsim {algorithm.sched_algo_name}",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if option_forward_generic and ret.returncode != 0:
        raise Exception(f'Bad robin return code ({ret.returncode})')
    elif not option_forward_generic and ret.returncode != 2:
        raise Exception(f'Batsim should complain')

def test_external_events(small_platform, mixed_workload, pybatsim_filler_events_algorithm, simple_events):
    externalevents(small_platform, mixed_workload, pybatsim_filler_events_algorithm, simple_events)


def test_generic_events_ok(small_platform, mixed_workload, pybatsim_filler_events_algorithm, generic_events):
    genericevents(small_platform, mixed_workload, pybatsim_filler_events_algorithm, generic_events, True)

def test_generic_events_fail(small_platform, mixed_workload, pybatsim_filler_events_algorithm, generic_events):
    genericevents(small_platform, mixed_workload, pybatsim_filler_events_algorithm, generic_events, False)

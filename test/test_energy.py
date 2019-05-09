#!/usr/bin/env python3
'''Energy tests.

These tests run batsim with energy-related features:
- SET_RESOURCE_STATE
- QUERY("consumed_energy")
'''
from helper import *
import json

def energy_model_instance():
    return {
        "power_sleep":9.75,
        "power_idle":95,
        "energy_switch_on":19030,
        "power_compute":190.738,
        "energy_switch_off":620,
        "time_switch_off":6.1,
        "pstate_sleep":13,
        "pstate_compute":0,
        "time_switch_on":152
    }

def energy(platform, workload, algorithm, batsim_args_energy, expected_robin_success, sched_additional_param):
    success_name = ''
    if not expected_robin_success: success_name = '-efail'
    test_name = f'energy-{algorithm.name}-{platform.name}-{workload.name}{success_name}'
    output_dir, robin_filename, schedconf_filename = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, batsim_args_energy)

    schedconf_content = energy_model_instance()
    schedconf_content.update({
        "output_dir": output_dir,
        "trace_output_filename": f'{output_dir}/sched_trace.csv',
    })
    schedconf_content.update(sched_additional_param)
    write_file(schedconf_filename, json.dumps(schedconf_content))

    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}' --variant_options_filepath '{schedconf_filename}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )
    instance.to_file(robin_filename)

    ret = run_robin(robin_filename)
    robin_success = ret.returncode == 0
    if robin_success != expected_robin_success: raise Exception(f'Bad robin success state (got={robin_success}, expected={expected_robin_success}, return_code={ret.returncode})')

def test_energy(energy_platform, small_workload, energy_algorithm):
    '''Classical energy tests. Run various energy-related actions.'''
    energy(energy_platform, small_workload, energy_algorithm, '--energy', True, {})

def test_energy_forgot_cli_flag(energy_platform, small_workload, energy_algorithm):
    '''Same as test_energy, but without the '--energy ' Batsim CLI flag. Execution should therefore fail.'''
    energy(energy_platform, small_workload, energy_algorithm, '', False, {})

def test_energy_minimal(energy_platform, energymini_workload, idle_sleeper_algorithm):
    '''Very small-scale tests.'''
    additional_sched_args = {
        "monitoring_period": 30,
        "idle_time_to_sedate": 0,
        "sedate_idle_on_classical_events": False,
        "ensured_sleep_time_lower_bound": 0,
        "ensured_sleep_time_upper_bound": 0,
    }
    energy(energy_platform, energymini_workload, idle_sleeper_algorithm, '--energy', True, additional_sched_args)

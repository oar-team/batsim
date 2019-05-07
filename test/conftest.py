#!/usr/bin/env python3
from collections import namedtuple
import glob
from os.path import abspath, basename, dirname, realpath

Workload = namedtuple('Workload', ['name', 'filename'])
Platform = namedtuple('Platform', ['name', 'filename'])
Algorithm = namedtuple('Platform', ['name', 'sched_implem', 'sched_algo_name'])

def pytest_generate_tests(metafunc):
    script_dir = dirname(realpath(__file__))
    batsim_dir = realpath(f'{script_dir}/..')

    # Platforms definition
    platforms_def = {
        "small": "small_platform.xml",
        "cluster512": "cluster512.xml",
        "energy128notopo": "energy_platform_homogeneous_no_net_128.xml",
        "energy128cluster": "cluster_energy_128.xml",
    }

    # Workloads definition
    workloads_def = {
        "delay1": "test_one_delay_job.json",
        "delays": "test_delays.json",
        "delaysequences": "test_sequence_delay.json",
        "energymini0": "test_energy_minimal_load0.json",
        "energymini50": "test_energy_minimal_load50.json",
        "energymini100": "test_energy_minimal_load100.json",
        "compute1": "test_one_computation_job.json",
        "computetot1": "test_one_computation_job_tot.json",
        "farfuture": "test_long_workload.json",
        "long": "test_batsim_paper_workload_seed1.json",
        "mixed": "test_various_profile_types.json",
        "samesubmittime": "test_same_submit_time.json",
        "smpicomp1": "test_smpi_compute_only.json",
        "smpicomp2": "test_smpi_compute_only_2_jobs.json",
        "smpimapping": "test_smpi_mapping.json",
        "smpimixed": "test_smpi_mixed_comp_comm.json",
        "smpicollectives": "test_smpi_collectives.json",
        "tuto1": "test_case_study1.json",
        "walltime": "test_walltime.json",
    }
    one_job_workloads = ["delay1", "compute1", "computetot1"]

    # Algorithms definition
    algorithms_def = {
        "easy": "easy_bf",
        "easyfast": "easy_bf_fast",
        "energywatcher": "energy_watcher",
        "idlesleeper": "energy_bf_idle_sleeper",
        "fcfs": "fcfs_fast",
        "filler": "filler",
        "random": "random",
        "rejecter": "rejecter",
        "sequencer": "sequencer",
        "sleeper": "sleeper",
        "submitter": "submitter",
    }
    basic_algorithms = ["fcfs", "easyfast", "filler"]

    if 'platform' in metafunc.fixturenames:
        platforms = [Platform(
            name=name,
            filename=abspath(f'{batsim_dir}/platforms/{filename}'))
            for name, filename in platforms_def.items()]
        metafunc.parametrize('platform', platforms)

    if 'one_job_workload' in metafunc.fixturenames:
        workloads = [Workload(
            name=name,
            filename=abspath(f'{batsim_dir}/workloads/{filename}'))
            for name, filename in workloads_def.items() if name in one_job_workloads]
        metafunc.parametrize('one_job_workload', workloads)

    if 'basic_algorithm' in metafunc.fixturenames:
        algos = [Algorithm(name=name, sched_implem='batsched', sched_algo_name=sched_algo_name)
            for name, sched_algo_name in algorithms_def.items() if name in basic_algorithms]
        print(algos)
        metafunc.parametrize('basic_algorithm', algos)

#!/usr/bin/env python3
import glob
import pytest
import subprocess
from collections import namedtuple
from os.path import abspath, basename, dirname, realpath
import helper

Workload = namedtuple('Workload', ['name', 'filename'])
Platform = namedtuple('Platform', ['name', 'filename'])
ExternalEvents = namedtuple('ExternalEvents', ['name', 'filename'])
Algorithm = namedtuple('Algorithm', ['name', 'sched_implem', 'sched_algo_name'])

RedisMode = namedtuple('RedisMode', ['name', 'enabled'])

def generate_platforms(platform_dir, definition, accepted_names):
    return [Platform(
        name=name,
        filename=abspath(f'{platform_dir}/{filename}'))
        for name, filename in definition.items() if name in accepted_names]

def generate_workloads(workload_dir, definition, accepted_names):
    return [Workload(
        name=name,
        filename=abspath(f'{workload_dir}/{filename}'))
        for name, filename in definition.items() if name in accepted_names]

def generate_external_events(external_events_dir, definition, accepted_names):
    return [ExternalEvents(
        name=name,
        filename=abspath(f'{external_events_dir}/{filename}'))
        for name, filename in definition.items() if name in accepted_names]

def generate_batsched_algorithms(definition, accepted_names):
    return [Algorithm(name=name, sched_implem='batsched', sched_algo_name=sched_algo_name)
        for name, sched_algo_name in definition.items() if name in accepted_names]

def generate_pybatsim_algorithms(definition, accepted_names):
    return [Algorithm(name=name, sched_implem='pybatsim', sched_algo_name=sched_algo_name)
        for name, sched_algo_name in definition.items() if name in accepted_names]

def pytest_addoption(parser):
    parser.addoption("--with-valgrind", action="store_true", default=False, help="runs batsim under valgrind memcheck analysis")

def pytest_generate_tests(metafunc):
    script_dir = dirname(realpath(__file__))
    batsim_dir = realpath(f'{script_dir}/..')
    platform_dir = realpath(f'{batsim_dir}/platforms')
    workload_dir = realpath(f'{batsim_dir}/workloads')
    external_events_dir = realpath(f'{batsim_dir}/events')
    helper.set_with_valgrind(metafunc.config.option.with_valgrind)

    #############################
    # Define the various inputs #
    #############################
    # Platforms
    platforms_def = {
        "small": "small_platform.xml",
        "cluster512": "cluster512.xml",
        "cluster512_pfs": "cluster512_pfs.xml",
        "energy128notopo": "energy_platform_homogeneous_no_net_128.xml",
        "energy128cluster": "cluster_energy_128.xml",
        "properties_platform": "properties_example.xml",
    }
    energy_platforms = ["energy128notopo", "energy128cluster"]

    # Workloads
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
        "genome": "GENOME.d.351024866.5.dax",
        "long": "test_batsim_paper_workload_seed1.json",
        "mixed": "test_various_profile_types.json",
        "samesubmittime": "test_same_submit_time.json",
        "smpicomp1": "test_smpi_compute_only.json",
        "smpicomp2": "test_smpi_compute_only_2_jobs.json",
        "smpimapping": "test_smpi_mapping.json",
        "smpimixed": "test_smpi_mixed_comp_comm.json",
        "smpicollectives": "test_smpi_collectives.json",
        "tuto1": "test_case_study1.json",
        "tutostencil": "test_tuto_stencil.json",
        "walltime": "test_walltime.json",
        "walltimesmpi": "test_walltime_smpi.json",
    }
    one_job_workloads = ["delay1", "compute1", "computetot1"]
    small_workloads = ["delays", "delaysequences", "mixed"]
    smpi_workloads = ["smpicomp1", "smpicomp2", "smpimapping", "smpimixed", "smpicollectives"]
    dynsub_workloads = ["delay1", "mixed"]
    moldable_perf_degradation_workloads = ['compute1', 'computetot1']
    energymini_workloads = ['energymini0', 'energymini50', 'energymini100']
    tuto_stencil_workloads = ['tutostencil']
    workflows = ['genome']

    # Algorithms
    algorithms_def = {
        "easy": "easy_bf",
        "easyfast": "easy_bf_fast",
        "energywatcher": "energy_watcher",
        "idlesleeper": "energy_bf_idle_sleeper",
        "fcfs": "fcfs_fast",
        "filler": "filler",
        "killer": "killer",
        "killer2": "killer2",
        "random": "random",
        "rejecter": "rejecter",
        "sequencer": "sequencer",
        "sleeper": "sleeper",
        "submitter": "submitter",
        "py_filler": "fillerSched",
        "py_filler_events": "fillerSchedWithEvents",
        "py_probe":"testProbesOneShot",
    }
    basic_algorithms = ["fcfs", "easyfast", "filler"]
    probe_algorithms = ['testProbesOneShot']
    energy_algorithms = ["sleeper"] #fixme: enable "energywatcher" once algo fixed
    metadata_algorithms = ['filler', 'submitter']

    # External Events
    external_events_def = {
        "simple": "test_events_4hosts.txt",
        "generic": "test_generic_event.txt",
    }

    ##########################################
    # Generate fixtures from the definitions #
    ##########################################
    # Platforms
    if 'platform' in metafunc.fixturenames:
        metafunc.parametrize('platform', generate_platforms(platform_dir, platforms_def, [key for key in platforms_def]))
    if 'energy_platform' in metafunc.fixturenames:
        metafunc.parametrize('energy_platform', generate_platforms(platform_dir, platforms_def, energy_platforms))
    if 'small_platform' in metafunc.fixturenames:
        metafunc.parametrize('small_platform', generate_platforms(platform_dir, platforms_def, ['small']))
    if 'cluster_platform' in metafunc.fixturenames:
        metafunc.parametrize('cluster_platform', generate_platforms(platform_dir, platforms_def, ['cluster512']))
    if 'cluster_pfs_platform' in metafunc.fixturenames:
        metafunc.parametrize('cluster_pfs_platform', generate_platforms(platform_dir, platforms_def, ['cluster512_pfs']))
    if 'properties_platform' in metafunc.fixturenames:
        metafunc.parametrize('properties_platform', generate_platforms(platform_dir, platforms_def, ['properties_platform']))

    # Workloads
    if 'workload' in metafunc.fixturenames:
        metafunc.parametrize('workload', generate_workloads(workload_dir, workloads_def, [key for key in workload_def]))
    if 'workflow' in metafunc.fixturenames:
        metafunc.parametrize('workflow', generate_workloads(workload_dir, workloads_def, [key for key in workflows]))
    if 'tuto_stencil_workload' in metafunc.fixturenames:
        metafunc.parametrize('tuto_stencil_workload', generate_workloads(workload_dir, workloads_def, [key for key in tuto_stencil_workloads]))
    if 'one_job_workload' in metafunc.fixturenames:
        metafunc.parametrize('one_job_workload', generate_workloads(workload_dir, workloads_def, one_job_workloads))
    if 'small_workload' in metafunc.fixturenames:
        metafunc.parametrize('small_workload', generate_workloads(workload_dir, workloads_def, small_workloads))
    if 'smpi_workload' in metafunc.fixturenames:
        metafunc.parametrize('smpi_workload', generate_workloads(workload_dir, workloads_def, smpi_workloads))
    if 'dynsub_workload' in metafunc.fixturenames:
        metafunc.parametrize('dynsub_workload', generate_workloads(workload_dir, workloads_def, dynsub_workloads))
    if 'moldable_perf_degradation_workload' in metafunc.fixturenames:
        metafunc.parametrize('moldable_perf_degradation_workload', generate_workloads(workload_dir, workloads_def, moldable_perf_degradation_workloads))
    if 'energymini_workload' in metafunc.fixturenames:
        metafunc.parametrize('energymini_workload', generate_workloads(workload_dir, workloads_def, energymini_workloads))
    if 'samesubmittime_workload' in metafunc.fixturenames:
        metafunc.parametrize('samesubmittime_workload', generate_workloads(workload_dir, workloads_def, ['samesubmittime']))
    if 'walltime_workload' in metafunc.fixturenames:
        metafunc.parametrize('walltime_workload', generate_workloads(workload_dir, workloads_def, ['walltime']))
    if 'walltime_smpi_workload' in metafunc.fixturenames:
        metafunc.parametrize('walltime_smpi_workload', generate_workloads(workload_dir, workloads_def, ['walltimesmpi']))
    if 'smpi_mapping_workload' in metafunc.fixturenames:
        metafunc.parametrize('smpi_mapping_workload', generate_workloads(workload_dir, workloads_def, ['smpimapping']))
    if 'long_workload' in metafunc.fixturenames:
        metafunc.parametrize('long_workload', generate_workloads(workload_dir, workloads_def, ['long']))
    if 'delaysequences_workload' in metafunc.fixturenames:
        metafunc.parametrize('delaysequences_workload', generate_workloads(workload_dir, workloads_def, ['delaysequences']))
    if 'mixed_workload' in metafunc.fixturenames:
        metafunc.parametrize('mixed_workload', generate_workloads(workload_dir, workloads_def, ['mixed']))

    # External Events
    if 'simple_events' in metafunc.fixturenames:
        metafunc.parametrize('simple_events', generate_external_events(external_events_dir, external_events_def, ['simple']))
    if 'generic_events' in metafunc.fixturenames:
        metafunc.parametrize('generic_events', generate_external_events(external_events_dir, external_events_def, ['generic']))

    # Algorithms
    if 'basic_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('basic_algorithm', generate_batsched_algorithms(algorithms_def, basic_algorithms))
    if 'energy_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('energy_algorithm', generate_batsched_algorithms(algorithms_def, energy_algorithms))
    if 'metadata_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('metadata_algorithm', generate_batsched_algorithms(algorithms_def, metadata_algorithms))
    if 'fcfs_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('fcfs_algorithm', generate_batsched_algorithms(algorithms_def, ['fcfs']))
    if 'filler_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('filler_algorithm', generate_batsched_algorithms(algorithms_def, ['filler']))
    if 'killer_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('killer_algorithm', generate_batsched_algorithms(algorithms_def, ['killer']))
    if 'killer2_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('killer2_algorithm', generate_batsched_algorithms(algorithms_def, ['killer2']))
    if 'submitter_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('submitter_algorithm', generate_batsched_algorithms(algorithms_def, ['submitter']))
    if 'random_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('random_algorithm', generate_batsched_algorithms(algorithms_def, ['random']))
    if 'idle_sleeper_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('idle_sleeper_algorithm', generate_batsched_algorithms(algorithms_def, ['idlesleeper']))

    if 'pybatsim_filler_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('pybatsim_filler_algorithm', generate_pybatsim_algorithms(algorithms_def, ['py_filler']))
    if 'pybatsim_filler_events_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('pybatsim_filler_events_algorithm', generate_pybatsim_algorithms(algorithms_def, ['py_filler_events']))
    if 'probe_algorithm' in metafunc.fixturenames:
        metafunc.parametrize('probe_algorithm', generate_pybatsim_algorithms(algorithms_def, ['py_probe']))

    # Misc. fixtures.
    if 'redis_mode' in metafunc.fixturenames:
        metafunc.parametrize('redis_mode', [RedisMode('redis', True), RedisMode('noredis', False)])

@pytest.fixture(scope="session", autouse=True)
def manage_redis_server(request):
    print('Trying to run a redis-server...')
    proc = subprocess.Popen('redis-server', stdout=subprocess.PIPE)
    try:
        out, _ = proc.communicate(timeout=1)
        if 'Address already in use' in str(out):
            print("Could not run redis-server (address already in use).")
            print("Assuming that the process using the TCP port is another redis-server instance and going on.")
        else:
            raise Exception("Could not run redis-server (unhandled reason), aborting.")
    except subprocess.TimeoutExpired:
        print('redis-server has been running for 1 second.')
        print('Assuming redis-server has started successfully and going on.')

    def on_finalize():
        print('Killing the spawned redis-server (if any)...')
        proc.kill()
    request.addfinalizer(on_finalize)

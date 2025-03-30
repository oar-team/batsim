import json
import os
import shlex
import subprocess
import pandas as pd

TEST_INSTANCE_TIMEOUT = int(os.getenv('TEST_INSTANCE_TIMEOUT', '5'))
PLATFORM_DIR = os.environ['PLATFORM_DIR']
WORKLOAD_DIR = os.environ['WORKLOAD_DIR']
EXTERNAL_EVENTS_DIR = os.environ['EXTERNAL_EVENTS_DIR']
EDC_DIR = os.environ['EDC_LD_LIBRARY_PATH']

def prepare_instance(name: str, test_root_dir: str, platform: str, edc: str, workload: str=None, external_event_files: [str]=None, edc_init_content: str='', use_json: bool=False, batsim_extra_args: list[str]=None):
    output_dir = f'{test_root_dir}/{name}'
    os.makedirs(output_dir, exist_ok=True)

    edc_init_filename = f'{output_dir}/edc-init'
    with open(edc_init_filename, 'w') as f:
        f.write(edc_init_content)

    batsim_cmd = [
        'batsim',
        '--export', f'{output_dir}/batout/',
        '--platform', f'{PLATFORM_DIR}/{platform}.xml',
        '--edc-library-file', f'{EDC_DIR}/lib{edc}.so', str(int(use_json)), edc_init_filename
    ]

    workload_file = None
    if workload is not None:
        workload_file = f'{WORKLOAD_DIR}/{workload}.json'
        batsim_cmd.extend([
            '--workload', workload_file
        ])

    ee_files = []
    if external_event_files is not None:
        for file in external_event_files:
            event_file = f'{EXTERNAL_EVENTS_DIR}/{file}.txt'
            ee_files.append(event_file)

        for ee_file in ee_files:
            batsim_cmd.extend([
                '--external-events', ee_file
            ])

    if batsim_extra_args is not None:
        batsim_cmd += batsim_extra_args

    batsim_cmd_filename = f'{output_dir}/batsim.sh'
    descriptor = os.open(path=batsim_cmd_filename, flags=os.O_WRONLY|os.O_CREAT|os.O_TRUNC, mode=0o700)
    with open(descriptor, 'w') as f:
        f.write(shlex.join(batsim_cmd) + '\n')

    return batsim_cmd, output_dir, workload_file, ee_files

def run_batsim(batsim_cmd, output_dir, timeout=None):
    used_timeout = TEST_INSTANCE_TIMEOUT
    if timeout is not None:
        used_timeout = timeout

    with open(f'{output_dir}/batsim.stdout', 'w') as outfile:
        with open(f'{output_dir}/batsim.stderr', 'w') as errfile:
            p = subprocess.run(batsim_cmd, stdout=outfile, stderr=errfile, timeout=used_timeout)
    return p

def compute_job_expected_state(row):
    if row['requested_time'] != -1 and row['requested_time'] < row['profile_expected_execution_time']:
        return 'COMPLETED_WALLTIME_REACHED'
    else:
        return 'COMPLETED_SUCCESSFULLY'

def compute_job_state_time_ok(row):
    return row['job_expected_state'] == row['final_state'] and (row['requested_time'] == -1 or row['job_expected_execution_time'] == row['execution_time'])

def check_job_duration_from_profile_expected_duration(workload_file, outdir):
    batjobs_filename = f'{outdir}/batout/jobs.csv'
    jobs = pd.read_csv(batjobs_filename)

    f = open(workload_file)
    batw = json.load(f)
    profile_names = list()
    expected_execution_times = list()

    for profile_name, profile in batw['profiles'].items():
        expected_execution_time = profile['expected_execution_time']
        profile_names.append(profile_name)
        expected_execution_times.append(expected_execution_time)

    expected_df = pd.DataFrame({'profile': profile_names, 'profile_expected_execution_time': expected_execution_times})

    df = jobs.merge(expected_df, on='profile', how='inner')
    df['job_expected_execution_time'] = df[['profile_expected_execution_time', 'requested_time']].min(axis=1)
    df['job_expected_state'] = df.apply(compute_job_expected_state, axis=1)
    df['all_good'] = df.apply(compute_job_state_time_ok, axis=1)

    with pd.option_context('display.max_rows', None, 'display.max_columns', None):
        if not df['all_good'].all():
            unexpected_df = df[df['all_good'] == False]
            printable_df = unexpected_df[['job_id', 'profile', 'requested_time', 'execution_time', 'job_expected_execution_time', 'final_state', 'job_expected_state']]
            print('Some jobs have an unexpected execution time or final state')
            print(printable_df)
            raise ValueError('Some jobs have an unexpected execution time or final state')
        else:
            print('All jobs are valid!')
            printable_df = df[['job_id', 'profile', 'requested_time', 'execution_time', 'job_expected_execution_time', 'final_state', 'job_expected_state']]
            print(printable_df)

def check_job_duration_from_job_expected_duration(workload_file, outdir):
    batjobs_filename = f'{outdir}/batout/jobs.csv'
    jobs = pd.read_csv(batjobs_filename)

    f = open(workload_file)
    batw = json.load(f)
    job_names = list()
    expected_execution_times = list()
    expected_states = list()

    for job in batw['jobs']:
        expected_execution_time = float(job['expected_execution_time'])
        expected_state = str(job['expected_state'])
        job_names.append(str(job['id']))
        expected_execution_times.append(expected_execution_time)
        expected_states.append(expected_state)

    expected_df = pd.DataFrame({
        'job_id': job_names,
        'job_expected_execution_time': expected_execution_times,
        'job_expected_state': expected_states,
    })

    df = jobs.merge(expected_df, on='job_id', how='inner')
    df['all_good'] = df.apply(compute_job_state_time_ok, axis=1)

    with pd.option_context('display.max_rows', None, 'display.max_columns', None):
        if not df['all_good'].all():
            unexpected_df = df[df['all_good'] == False]
            printable_df = unexpected_df[['job_id', 'profile', 'requested_time', 'execution_time', 'job_expected_execution_time', 'final_state', 'job_expected_state']]
            print('Some jobs have an unexpected execution time or final state')
            print(printable_df)
            raise ValueError('Some jobs have an unexpected execution time or final state')
        else:
            print('All jobs are valid!')
            printable_df = df[['job_id', 'profile', 'requested_time', 'execution_time', 'job_expected_execution_time', 'final_state', 'job_expected_state']]
            print(printable_df)

def run_instance_check_job_duration_and_state(test_root_dir, platform, workload, instance_name, edc, batsim_timeout=None):
    batcmd, outdir, workload_file, _ = prepare_instance(instance_name, test_root_dir, platform, edc, workload)
    p = run_batsim(batcmd, outdir, timeout=batsim_timeout)
    assert p.returncode == 0

    check_job_duration_from_profile_expected_duration(workload_file, outdir)

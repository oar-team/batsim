#!/usr/bin/env python3
'''Job metadata tests.

These tests checks whether metadata set by the scheduled is preserved in
Batsim output files.
'''
import numpy
import json
import pandas as pd
import pytest
from helper import *

def check_present_bool(row):
    if row['metadata'] == "":
        return False

    if isinstance(row['metadata'], float) or isinstance(row['metadata'], numpy.float64):
        if not numpy.isfinite(row['metadata']):
            return False

    return True

def check_present(row):
    return int(check_present_bool(row))

def check_absent(row):
    return int(not check_present_bool(row))

# Whether job metadata should be set by the scheduler or not.
JobMetadataMode = namedtuple('JobMetadataMode', ['name', 'should_be_set'])

def metadata(platform, workload, algorithm, metadata_mode):
    test_name = f'{metadata_mode.name}-job-metadata-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, schedconf_filename = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batparams = ''
    if algorithm.name == 'submitter': batparams = '--enable-dynamic-jobs'

    schedconf_content = {
        "set_job_metadata": metadata_mode.should_be_set,
    }
    write_file(schedconf_filename, json.dumps(schedconf_content))

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, batparams)
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}' --variant_options_filepath '{schedconf_filename}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    # Analyze Batsim results to check whether jobs contain metadata.
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)

    if metadata_mode.should_be_set:
        jobs['valid'] = jobs.apply(check_present, axis=1)
    else:
        jobs['valid'] = jobs.apply(check_absent, axis=1)

    if jobs['valid'].sum() != len(jobs):
        print('Some jobs are invalid!')
        print(jobs[['job_id', 'metadata', 'valid']])
        raise Exception(f'Unexpected metadata value for some jobs. scheduler_should_set_metadata={metadata_mode.should_be_set}')
    else:
        print('All jobs are valid!')
        print(jobs[['job_id', 'metadata', 'valid']])

@pytest.mark.parametrize("metadata_mode", [JobMetadataMode('set', True), JobMetadataMode('do-not-set', False)])

def test_metadata(small_platform, small_workload, metadata_algorithm, metadata_mode):
    metadata(small_platform, small_workload, metadata_algorithm, metadata_mode)

#!/usr/bin/env python3
'''Job submission time order tests.

These tests checks that Batsim submits jobs in a chronologically and
deterministic time in the protocol.
Jobs should be submitted sorted by (submission_time, job_id).
'''
import pandas as pd
from helper import *

def submitorder(platform, workload, algorithm):
    test_name = f'submitorder-{algorithm.name}-{platform.name}-{workload.name}'
    output_dir, robin_filename, _ = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batcmd = gen_batsim_cmd(platform.filename, workload.filename, output_dir, "")
    instance = RobinInstance(output_dir=output_dir,
        batcmd=batcmd,
        schedcmd=f"batsched -v '{algorithm.sched_algo_name}'",
        simulation_timeout=30, ready_timeout=5,
        success_timeout=10, failure_timeout=0
    )

    instance.to_file(robin_filename)
    ret = run_robin(robin_filename)
    if ret.returncode != 0: raise Exception(f'Bad robin return code ({ret.returncode})')

    ######################################################################
    # Retrieve observed submission order — as it appears in Batsim logs. #
    ######################################################################
    # Read file.
    batlog_filename = f'{output_dir}/log/batsim.log'
    batlog_content = open(batlog_filename, 'r').read()
    # Parse messages, then events.
    messages = parse_proto_messages_from_batsim(batlog_content)
    events = retrieve_proto_events(messages)
    # Filter JOB_SUBMITTED events.
    submission_events = [e for e in events if e['type'] == 'JOB_SUBMITTED']
    observed_submission_order = [e['data']['job_id'].split('!')[1] for e in submission_events]

    #################################################
    # Compute the theoretical job submission order. #
    #################################################
    batjobs_filename = f'{output_dir}/batres_jobs.csv'
    jobs = pd.read_csv(batjobs_filename)
    jobs.sort_values(by=['submission_time', 'job_id'], inplace=True)
    expected_submission_order = [str(x) for x in jobs['job_id']]

    ########################################
    # Compare observed and expected order. #
    ########################################
    print('Job submission order:')
    print('  observed:', observed_submission_order)
    print('  expected:', expected_submission_order)
    if observed_submission_order != expected_submission_order:
        print('MISMATCH!')
        raise Exception('Job submission order mismatch')

def test_submitorder1(small_platform, small_workload, fcfs_algorithm):
    submitorder(small_platform, small_workload, fcfs_algorithm)

def test_submitorder2(small_platform, samesubmittime_workload, fcfs_algorithm):
    submitorder(small_platform, samesubmittime_workload, fcfs_algorithm)

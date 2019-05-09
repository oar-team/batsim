#!/usr/bin/env python3
'''Dynamic submission tests.

These tests run batsim with dynamic submission enabled (--enable-dynamic-jobs).
This mechanism lets the scheduler submit and execute its own jobs at runtime.
'''
from collections import namedtuple
from helper import *
import json
import pytest

# Whether Redis should be used or not.
RedisMode = namedtuple('RedisMode', ['name', 'enabled'])
# Whether Batsim should acknowledge dynamic submissions or not.
DynamicSubmissionAcknowledgementMode = namedtuple('DynamicSubmissionAcknowledgementMode', ['name', 'enabled'])
# The number of jobs that should be submitted dynamically.
DynamicJobCount = namedtuple('DynamicJobCount', ['name', 'job_count'])
# Submission parameters for the scheduler.
SchedulerSubmissionMode = namedtuple('SchedulerSubmissionMode',
    ['name', 'increase_jobs_duration', 'send_profile_if_already_sent', 'send_profiles_in_separate_event'])

@pytest.mark.parametrize("dyn_sub_ack_mode",
    [DynamicSubmissionAcknowledgementMode('ack', True),
     DynamicSubmissionAcknowledgementMode('noack', False)])
@pytest.mark.parametrize("nb_dynamic_jobs", [DynamicJobCount(f'{n}jobs', n) for n in [0,1,10]])
@pytest.mark.parametrize("sched_submission_mode", [
    SchedulerSubmissionMode('diff-profiles-separatep', True, True, True),
    SchedulerSubmissionMode('same-profiles-noresend-separatep', False, False, True)])

def test_dynsubmit(small_platform, dynsub_workload, submitter_algorithm,
    redis_mode, dyn_sub_ack_mode, nb_dynamic_jobs, sched_submission_mode):
    dynsubmit(small_platform, dynsub_workload, submitter_algorithm,
        redis_mode, dyn_sub_ack_mode, nb_dynamic_jobs, sched_submission_mode)

def dynsubmit(platform, workload, algorithm,
    redis_mode, dyn_sub_ack_mode, nb_dynamic_jobs, sched_submission_mode):
    test_name = f'dynsubmit-{workload.name}-{sched_submission_mode.name}-{redis_mode.name}-{dyn_sub_ack_mode.name}-{nb_dynamic_jobs.name}'
    output_dir, robin_filename, schedconf_filename = init_instance(test_name)

    if algorithm.sched_implem != 'batsched': raise Exception('This test only supports batsched for now')

    batparams = "--enable-dynamic-jobs"
    if redis_mode.enabled == True: batparams = batparams + " --enable-redis"
    if dyn_sub_ack_mode.enabled == True: batparams = batparams + " --acknowledge-dynamic-jobs"

    schedconf_content = {
        "nb_jobs_to_submit": nb_dynamic_jobs.job_count,
        "increase_jobs_duration": sched_submission_mode.increase_jobs_duration,
        "send_profile_if_already_sent": sched_submission_mode.send_profile_if_already_sent,
        "send_profiles_in_separate_event": sched_submission_mode.send_profiles_in_separate_event,
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

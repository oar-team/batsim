#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with an EDC that checks that all expected external events are received around the expected time.
'''
import inspect
import os
import subprocess
import pytest

from helper import prepare_instance, run_batsim, EXTERNAL_EVENTS_DIR

MOD_NAME = __name__.replace('test_', '', 1)

@pytest.fixture(scope="module", params=[
    ("e1", ["test_generic_events"]),
    ("e2", ["test_generic_events2"]),
    ("e1e2", ["test_generic_events", "test_generic_events2"]),
])
def parameters(request):
    return request.param

def test_external_events(test_root_dir, parameters):
    platform = 'small_platform'
    workload = 'test_one_delay_job'
    param_name, event_filenames = parameters
    func_name = inspect.currentframe().f_code.co_name.replace('test_', '', 1)
    instance_name = f'{MOD_NAME}-{func_name}-{param_name}'

    edc_init_args = {
        "external_event_filenames": [ f"{EXTERNAL_EVENTS_DIR}/{f}.txt" for f in event_filenames ]
    }

    batcmd, outdir, _, _ = prepare_instance(instance_name, test_root_dir, platform, 'external-event-check', workload, external_event_files=event_filenames, edc_init_content=edc_init_args)
    p = run_batsim(batcmd, outdir)
    assert p.returncode == 0

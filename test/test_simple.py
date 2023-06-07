#!/usr/bin/env python3
'''Simple tests.

These tests run batsim with most basic features (execute jobs, reject jobs).
'''
import os
import subprocess
import pytest

@pytest.fixture(scope="module")
def platform():
    return 'small_platform.xml'

@pytest.fixture(scope="module")
def workload():
    return 'test_delays.json'

@pytest.fixture(scope="module", params=['0', '1'])
def use_json(request):
    return request.param

def test_reject_all_jobs(platform, workload, use_json):
    platform_dir = os.environ['PLATFORM_DIR']
    workload_dir = os.environ['WORKLOAD_DIR']
    edc_dir = os.environ['EDC_LD_LIBRARY_PATH']
    p = subprocess.run(['batsim',
        '-p', f'{platform_dir}/{platform}',
        '-w', f'{workload_dir}/{workload}',
        '--edc-library-str', f'{edc_dir}/librejecter.so', use_json, ''
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=3, encoding='utf-8')
    assert p.returncode == 0

def test_execute_jobs_one_by_one(platform, workload, use_json):
    platform_dir = os.environ['PLATFORM_DIR']
    workload_dir = os.environ['WORKLOAD_DIR']
    edc_dir = os.environ['EDC_LD_LIBRARY_PATH']
    p = subprocess.run(['batsim',
        '-p', f'{platform_dir}/{platform}',
        '-w', f'{workload_dir}/{workload}',
        '--edc-library-str', f'{edc_dir}/libexec1by1.so', use_json, ''
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=3, encoding='utf-8')
    assert p.returncode == 0

def test_fcfs(platform, workload, use_json):
    platform_dir = os.environ['PLATFORM_DIR']
    workload_dir = os.environ['WORKLOAD_DIR']
    edc_dir = os.environ['EDC_LD_LIBRARY_PATH']
    out_dir = f'/tmp/fcfs-{use_json}'
    os.makedirs(out_dir, exist_ok=True)

    with open(f'{out_dir}/batsim.out', 'w') as outfile:
        with open(f'{out_dir}/batsim.err', 'w') as errfile:
            p = subprocess.run(['batsim',
                '-p', f'{platform_dir}/{platform}',
                '-w', f'{workload_dir}/{workload}',
                '--edc-library-str', f'{edc_dir}/libfcfs.so', use_json, ''
            ], stdout=outfile, stderr=errfile, timeout=3, encoding='utf-8')
            assert p.returncode == 0

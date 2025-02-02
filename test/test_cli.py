#!/usr/bin/env python3
'''CLI tests.

These tests focus on Batsim CLI (Command-Line Interface) options.
'''
import subprocess

def test_cli_help():
    p = subprocess.run(['batsim', '--help'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=3, encoding='utf-8')
    case_desc = 'when launched with --help'
    assert p.returncode == 0, f'batsim should return 0 {case_desc}'
    assert p.stderr == '', f'batsim should not output anything on stderr {case_desc}'
    assert p.stdout != '', f'batsim should output something on stdout {case_desc}'

def test_cli_no_arg():
    p = subprocess.run(['batsim'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding='utf-8')
    case_desc = 'when launched without any argument'
    assert p.returncode != 0, f'batsim should not return 0 {case_desc}'
    assert p.stdout == '', f'batsim should not output anything on stdout {case_desc}'
    assert p.stderr != '', f'batsim should output something on stderr {case_desc}'

def test_cli_version():
    p = subprocess.run(['batsim', '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=3, encoding='utf-8')
    case_desc = 'when launched with --version'
    assert p.returncode == 0, f'batsim should return 0 {case_desc}'
    assert p.stderr == '', f'batsim should not output anything on stderr {case_desc}'
    assert p.stdout != '', f'batsim should output something on stdout {case_desc}'

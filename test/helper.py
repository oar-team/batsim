#!/usr/bin/env python3
import os
import os.path
import subprocess
from collections import namedtuple

class RobinInstance(object):
    def __init__(self, output_dir, batcmd, schedcmd, simulation_timeout, ready_timeout, success_timeout, failure_timeout):
        self.output_dir = output_dir
        self.batcmd = batcmd
        self.schedcmd = schedcmd
        self.simulation_timeout = simulation_timeout
        self.ready_timeout = ready_timeout
        self.success_timeout = success_timeout
        self.failure_timeout = failure_timeout

    def to_yaml(self):
        # Manual dump to avoid dependencies
        return f'''output-dir: '{self.output_dir}'
batcmd: "{self.batcmd}"
schedcmd: "{self.schedcmd}"
simulation-timeout: {self.simulation_timeout}
ready-timeout: {self.ready_timeout}
success-timeout: {self.success_timeout}
failure-timeout: {self.failure_timeout}
'''

    def to_file(self, filename):
        create_dir_rec_if_needed(os.path.dirname(filename))
        write_file(filename, self.to_yaml())

def gen_batsim_cmd(platform, workload, output_dir, more_flags):
    return f"batsim -p '{platform}' -w '{workload}' -e '{output_dir}/batres' {more_flags}"

def write_file(filename, content):
    file = open(filename, "w")
    file.write(content)
    file.close()

def create_dir_rec_if_needed(dirname):
    if not os.path.exists(dirname):
            os.makedirs(dirname)

def run_robin(filename):
    return subprocess.run(['robin', filename])

def init_instance(test_name):
    output_dir = os.path.abspath(f'test-out/{test_name}')
    robin_filename = os.path.abspath(f'{output_dir}/instance.yaml')
    schedconf_filename = os.path.abspath(f'{output_dir}/schedconf.json')

    create_dir_rec_if_needed(output_dir)

    return (output_dir, robin_filename, schedconf_filename)

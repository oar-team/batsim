#!/usr/bin/env python3
import json
import os
import os.path
import re
import subprocess
from collections import namedtuple

glob_with_valgrind = False

def set_with_valgrind(with_valgrind):
    global glob_with_valgrind
    glob_with_valgrind = with_valgrind

class RobinInstance(object):
    def __init__(self, output_dir, batcmd, schedcmd, simulation_timeout, ready_timeout, success_timeout, failure_timeout):
        global glob_with_valgrind
        self.output_dir = output_dir
        self.batcmd = batcmd
        self.schedcmd = schedcmd
        self.simulation_timeout = simulation_timeout * (1,20)[glob_with_valgrind]
        self.ready_timeout = ready_timeout
        self.success_timeout = success_timeout * (1,20)[glob_with_valgrind]
        self.failure_timeout = failure_timeout * (1,20)[glob_with_valgrind]

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

def gen_batsim_cmd(platform, workload, output_dir, more_flags, is_workload=True):
    wload_flag = "-w" if is_workload else "-W"
    batcmd = f"batsim -p '{platform}' {wload_flag} '{workload}' -e '{output_dir}/batres' {more_flags}"
    if glob_with_valgrind:
        return f"valgrind --leak-check=yes --xml=yes --xml-file='{output_dir}/memcheck.xml' {batcmd}"
    else:
        return batcmd

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

def parse_proto_messages_from_batsim(batsim_log):
    '''Returns a list of messages sent by Batsim (respecting the trace order).'''
    messages = []
    for line in batsim_log.splitlines():
        match = re.search(r"""Sending '(.*)'""", line)
        if match:
            messages.append(json.loads(match.group(1)))
    return messages

def retrieve_proto_events(messages):
    '''Returns a list of events from a list of protocol messages.'''
    events = []
    for msg in messages:
        if len(msg['events']) > 0:
            events.extend(msg['events'])
    return events

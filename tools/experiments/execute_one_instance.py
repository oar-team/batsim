#!/usr/bin/python2

import argparse
import json
import os
import execo

class InstanceExecutionData:
    def __init__(self, batsim_process, sched_process):
        self.batsim_process = batsim_process
        self.sched_process = sched_process

class BatsimLifecycleHandler(execo.process.ProcessLifecycleHandler):
    def __init__(self):
        pass
    def set_execution_data(execution_data)
        self.execution_data = execution_data
    def end(self, process):
        if not process.finished_ok:
            if self.execution_data.sched_process.running:
                self.execution_data.sched_process.kill(auto_force_kill_timeout = 1)

class SchedLifecycleHandler(execo.process.ProcessLifecycleHandler):
    def __init__(self):
        pass
    def set_execution_data(execution_data)
        self.execution_data = execution_data
    def end(self, process):
        if not process.finished_ok:
            if self.execution_data.sched_process.running:
                self.execution_data.sched_process.kill(auto_force_kill_timeout = 1)

def evaluate_variables_in_string(string,
                                 variables):
    res = string
    for var in variables:
        dollar_var = '$' + var
        dollar_var_brackets = '${' + var + '}'
        res = res.replace(dollar_var, variables[var])
        res = res.replace(dollar_var_brackets, variables[var])
    return res

def execute_command(working_directory,
                    command,
                    variables):
    pass

def socket_in_use(sock):
    return sock in open('/proc/net/unix').read()

def wait_for_batsim_to_open_connection(execution_data,
                                       sock='/tmp/bat_socket',
                                       timeout=60,
                                       seconds_to_sleep=0.1):
    remaining_time = timeout
    while remaining_time > 0 and not socket_in_use(sock) and not execution_data.batsim_process.ended:
        time.sleep(seconds_to_sleep)
        remaining_time -= seconds_to_sleep

    return remainig_time > 0

def execute_one_instance(working_directory,
                         batsim_command,
                         sched_command,
                         variables,
                         timeout = None):
    batsim_command_eval = evaluate_variables_in_string(batsim_command, variables)
    sched_command_eval = evaluate_variables_in_string(sched_command, variables)

    batsim_lifecycle_handler = BatsimLifecycleHandler()
    sched_lifecycle_handler = SchedLifecycleHandler()

    batsim_process = execo.process.Process(batsim_command_eval,
                                           kill_subprocesses=True,
                                           name="batsim_process",
                                           cwd = working_directory,
                                           timeout = timeout,
                                           lifecycle_handlers = batsim_lifecycle_handler)

    sched_process = execo.process.Process(sched_command_eval,
                                          kill_subprocesses=True,
                                          name="sched_process",
                                          cwd = working_directory,
                                          timeout = timeout,
                                          lifecycle_handlers = sched_lifecycle_handler)

    execution_data = InstanceExecutionData(batsim_process, sched_process)

    batsim_lifecycle_handler.set_execution_data(execution_data)
    sched_lifecycle_handler.set_execution_data(execution_data)

    # Batsim starts
    batsim_process.start()

    # Wait for Batsim to create the socket
    wait_for_batsim_to_open_connection(execution_data, timeout = timeout)

    # If Batsim did not crash, let the scheduler be run
    if batsim_process.running:
        sched_process.start()

        # Both processes are executed, let's wait for their termination.
        # Thanks to LifecycleHandlers, if any processes stops without being
        # successful, the other process would be killed to avoid reaching
        # timeout
        batsim_process.wait()
        sched_process.wait()

        TODO: handle execution output
        TODO: leave the script with a non-zero error code on failure

def main():
    # Program parameters parsing
    p = argparse.ArgumentParser(description = 'Launches one Batsim instance. '
                                'One instance is defined by a workload, '
                                'a platform and a scheduling algorithm')

    p.add_argument('instance_description_json_filename',
                   type = str,
                   help = 'The name of the JSON file which describes the instance')

    p.add_argument("-v", "--verbose",
                   help = "increase output verbosity",
                   action = "store_true")

    args = p.parse_args()

    # Let's read the JSON file content to get the real parameters

    json_file = open(args.instance_description_json_filename, 'r')
    json_data = json.load(json_file,
                          object_pairs_hook = OrderedDict)

    working_directory = os.getcwd()
    commands_before_execution = []
    commands_after_execution = []
    variables = {}
    timeout = None

    assert('batsim_command' in json_data)
    assert('sched_command' in json_data)

    batsim_command = str(json_data['batsim_command'])
    sched_command = str(json_data['sched_command'])

    if 'working_directory' in json_data:
        working_directory = str(json_data['working_directory'])
    if 'commands_before_execution' in json_data:
        commands_before_execution = [str(command) for command in json_data['commands_before_execution']]
    if 'commands_after_execution' in json_data:
        commands_after_execution = [str(command) for command in json_data['commands_after_execution']]
    if 'variables' in json_data:
        variables = dict(json_data['variables'])
    if 'timeout' in json_data:
        timeout = float(json_data['timeout'])

    # Let the execution be started
    # Commands before instance execution
    for command in commands_before_execution:
        execute_command(working_directory = working_directory,
                        command = command,
                        variables = variables)

    # Instance execution
    execute_one_instance(working_directory = working_directory,
                         batsim_command = batsim_command,
                         sched_command = sched_command,
                         variables = variables,
                         timeout = timeout)

    # Commands after instance execution
    for command in commands_after_execution:
        execute_command(working_directory = working_directory,
                        command = command,
                        variables = variables)

if __name__ == '__main__':
    main()

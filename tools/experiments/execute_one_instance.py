#!/usr/bin/python2

import argparse
import yaml
import os
import sys
import time
import shlex
from execo import *

def find_socket_from_batsim_command(batsim_command):
    split_command = shlex.split(batsim_command)

    batparser = argparse.ArgumentParser(prog = split_command[0],
                                        description = 'Batsim command parser',
                                        add_help = False)

    batparser.add_argument("platform")
    batparser.add_argument("workload")

    batparser.add_argument("-e", "--export", type=str, default="out")
    batparser.add_argument("-h", "--allow-space-sharing", action='store_true')
    batparser.add_argument("-l", "--limit-machine-count", type=int, default=-1)
    batparser.add_argument("-L", "--limit-machine-count-by-workload", action='store_true')
    batparser.add_argument("-m", "--master-host", default="master_host")
    batparser.add_argument("-p", "--energy-plugin", action='store_true')
    batparser.add_argument("-q", "--quiet", action='store_true')
    batparser.add_argument("-s", "--socket", type=str, default="/tmp/bat_socket")
    batparser.add_argument("-t", "--process-tracing", action='store_true')
    batparser.add_argument("-T", "--disable-schedule-tracing", action='store_true')
    batparser.add_argument("-v", "--verbosity", type=str, default="information")

    batargs = batparser.parse_args(split_command[1:])

    return batargs.socket

def write_string_into_file(string, filename):
    f = open(filename, 'w')
    f.write(string)
    f.close()

def create_dir_if_not_exists(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

class InstanceExecutionData:
    def __init__(self, batsim_process, batsim_socket, sched_process,
                 timeout, output_directory):
        self.batsim_process = batsim_process
        self.sched_process = sched_process
        self.batsim_socket = batsim_socket
        self.timeout = timeout
        self.output_directory = output_directory
        self.nb_started = 0
        self.nb_finished = 0
        self.failure = False

class BatsimLifecycleHandler(ProcessLifecycleHandler):
    def set_execution_data(self, execution_data):
        self.execution_data = execution_data
    def start(self, process):
        logger.info("Batsim started")

        # Wait for Batsim to create the socket
        if wait_for_batsim_to_open_connection(self.execution_data,
                                              timeout = self.execution_data.timeout,
                                              sock = self.execution_data.batsim_socket):
            # Launches the scheduler
            logger.info("Batsim's socket {} is now opened".format(self.execution_data.batsim_socket))
            self.execution_data.sched_process.start()
        else:
            self.execution_data.failure = True
        self.execution_data.nb_started += 1

    def end(self, process):
        logger.debug('process : {}'.format(id(process)))
        # Let's write stdout and stderr to files
        write_string_into_file(process.stdout, '{output_dir}/batsim.stdout'.format(
                                output_dir = self.execution_data.output_directory))
        write_string_into_file(process.stderr, '{output_dir}/batsim.stderr'.format(
                                output_dir = self.execution_data.output_directory))

        # Let's check whether the process was successful
        if (process.exit_code != 0) or process.timeouted or process.killed or process.error:
            self.execution_data.failure = True

            if process.killed:
                logger.error("Batsim ended unsucessfully (killed)")
            elif process.error:
                logger.error("Batsim ended unsucessfully (error: {})".format(process.error_reason))
            elif process.timeouted:
                logger.error("Batsim ended unsucessfully (reached timeout)")
            elif process.exit_code != 0:
                logger.error("Batsim ended unsucessfully (exit_code = {})".format(process.exit_code))
            else:
                logger.error("Batsim ended unsucessfully (unknown reason)")
            if self.execution_data.sched_process.running:
                logger.warning("Killing Sched")
                self.execution_data.sched_process.kill(auto_force_kill_timeout = 1)
        else:
            logger.info("Batsim ended successfully")
        self.execution_data.nb_finished += 1

class SchedLifecycleHandler(ProcessLifecycleHandler):
    def set_execution_data(self, execution_data):
        self.execution_data = execution_data
    def start(self, process):
        logger.info("Sched started")
        self.execution_data.nb_started += 1
    def end(self, process):
        # Let's write stdout and stderr to files
        write_string_into_file(process.stdout, '{output_dir}/sched.stdout'.format(
                                output_dir = self.execution_data.output_directory))
        write_string_into_file(process.stderr, '{output_dir}/sched.stderr'.format(
                                output_dir = self.execution_data.output_directory))

        # Let's check whether the process was successful
        if (process.exit_code != 0) or process.timeouted or process.killed or process.error:
            self.execution_data.failure = True

            if process.killed:
                logger.error("Sched ended unsucessfully (killed)")
            elif process.error:
                logger.error("Sched ended unsucessfully (error: {})".format(process.error_reason))
            elif process.timeouted:
                logger.error("Sched ended unsucessfully (reached timeout)")
            elif process.exit_code != 0:
                logger.error("Sched ended unsucessfully (exit_code = {})".format(process.exit_code))
            else:
                logger.error("Sched ended unsucessfully (unknown reason)")

            if self.execution_data.batsim_process.running:
                logger.warning("Killing Batsim")
                self.execution_data.batsim_process.kill(auto_force_kill_timeout = 1)
        else:
            logger.info("Sched ended successfully")
        self.execution_data.nb_finished += 1

def evaluate_variables_in_string(string,
                                 variables):
    res = string
    #print(res)
    for var_name in variables:
        var_val = variables[var_name]
        #print('name={},val={}'.format(var_name, var_val))
        res = str(res)
        if isinstance(var_val, tuple) or isinstance(var_val, list):
            for list_i in range(len(var_val)):
                arobase_var_brackets = '@' + str(list_i) + '{' + var_name + '}'
                #print('{} -> {}'.format(arobase_var_brackets, var_val[list_i]))
                #print('before =1, type(res)=', type(res))
                res = res.replace(arobase_var_brackets, var_val[list_i])
                #print('after =1,  type(res)=', type(res))
                #print('after =1,  type(res)=', type(res))
        elif not isinstance(var_val, int) and not isinstance(var_val, float):
            dollar_var_brackets = '${' + var_name + '}'
            #print('{} -> {}'.format(dollar_var_brackets, var_val))
            #print('before =2, type(res)=', type(res))
            res = res.replace(dollar_var_brackets, var_val)
            #print('after =2,  type(res)=', type(res))
            #print('after =2,  type(res)=', type(res))
    #print(res)
    #print('')
    return res

def execute_command(working_directory,
                    command,
                    variables):
    return true
    # TODO

def socket_in_use(sock):
    return sock in open('/proc/net/unix').read()

def wait_for_batsim_to_open_connection(execution_data,
                                       sock='/tmp/bat_socket',
                                       timeout=60,
                                       seconds_to_sleep=0.1):
    if timeout == None:
        timeout = 3600
    remaining_time = timeout
    while remaining_time > 0 and not socket_in_use(sock) and not execution_data.batsim_process.ended:
        time.sleep(seconds_to_sleep)
        remaining_time -= seconds_to_sleep

    return socket_in_use(sock)

def wait_for_batsim_socket_to_be_usable(sock = '/tmp/bat_socket',
                                        timeout = 60,
                                        seconds_to_sleep = 0.1):
    if timeout == None:
        timeout = 3600
    remaining_time = timeout
    while remaining_time > 0 and socket_in_use(sock):
        time.sleep(seconds_to_sleep)
        remaining_time -= seconds_to_sleep
    return remaining_time > 0

def execute_one_instance(working_directory,
                         output_directory,
                         batsim_command,
                         sched_command,
                         variables,
                         timeout = None):

    logger.info('Batsim command: "{}"'.format(batsim_command))
    logger.info('Sched command: "{}"'.format(sched_command))

    # Let's create the output directory if it does not exist
    create_dir_if_not_exists(output_directory)

    # Let's create lifecycle handlers, which will manage what to do on process's events
    batsim_lifecycle_handler = BatsimLifecycleHandler()
    sched_lifecycle_handler = SchedLifecycleHandler()

    # If batsim's socket is still opened, let's wait for it to close
    batsim_socket = find_socket_from_batsim_command(batsim_command)
    socket_usable = wait_for_batsim_socket_to_be_usable(sock = batsim_socket,
                                                        timeout = timeout)
    assert(socket_usable)
    logger.info("Socket {sock} is now usable".format(sock = batsim_socket))

    # Let's create the execo processes
    batsim_process = Process(cmd = batsim_command,
                             kill_subprocesses = True,
                             name = "batsim_process",
                             cwd = working_directory,
                             timeout = timeout,
                             lifecycle_handlers = [batsim_lifecycle_handler])

    sched_process = Process(cmd = sched_command,
                            kill_subprocesses = True,
                            name = "sched_process",
                            cwd = working_directory,
                            timeout = timeout,
                            lifecycle_handlers = [sched_lifecycle_handler])

    # Let's create a shared execution data, which will be given to LC handlers
    execution_data = InstanceExecutionData(batsim_process = batsim_process,
                                           sched_process = sched_process,
                                           batsim_socket = batsim_socket,
                                           timeout = timeout,
                                           output_directory = output_directory)

    batsim_lifecycle_handler.set_execution_data(execution_data)
    sched_lifecycle_handler.set_execution_data(execution_data)

    # Batsim starts
    batsim_process.start()

    # Wait for processes' termination
    while (execution_data.failure == False) and (execution_data.nb_finished < 2):
        sleep(0.2)

    while (execution_data.nb_finished < execution_data.nb_started):
        sleep(0.2)

    if execution_data != None:
        success = not execution_data.failure
    else:
        success = True

    logger.debug('batsim process : {}'.format(id(batsim_process)))
    return success

def main():
    script_description = '''
Lauches one Batsim instance.
An instance can be represented by a tuple (platform, workload, algorithm).
It is described in a YAML file, which is the parameter of this script.

Examples of such input files can be found in the subdirectory instance_examples.
'''


    # Program parameters parsing
    p = argparse.ArgumentParser(description = script_description)

    p.add_argument('instance_description_filename',
                   type = str,
                   help = 'The name of the YAML file which describes the instance. '
                          'Beware, this argument is not subjected to the working '
                          'directory parameter.')

    p.add_argument('-wd', '--working_directory',
                   type = str,
                   default = None,
                   help = 'If set, the instance will be executed in the given '
                          'directory. This value has higher priority than the '
                          'one that might be given in the description file. '
                          'If unset, the script working directory is used instead')

    p.add_argument('-od', '--output_directory',
                   type = str,
                   default = None,
                   help = 'If set, the outputs of the current script will be '
                          'put into the given directory. This value has higher '
                          'priority than the one that might be given in the '
                          'description file. If a value is set, it might be '
                          'either absolute or relative to the working directory. '
                          ' If unset, the working directory is used instead')

    args = p.parse_args()

    # Let's read the YAML file content to get the real parameters
    desc_file = open(args.instance_description_filename, 'r')
    desc_data = yaml.load(desc_file)

    working_directory = os.getcwd()
    output_directory = os.getcwd()
    commands_before_execution = []
    commands_after_execution = []
    variables = {}
    timeout = None

    assert('batsim_command' in desc_data)
    assert('sched_command' in desc_data)

    batsim_command = str(desc_data['batsim_command'])
    sched_command = str(desc_data['sched_command'])

    if 'working_directory' in desc_data:
        working_directory = str(desc_data['working_directory'])
    if 'output_directory' in desc_data:
        output_directory = str(desc_data['output_directory'])
    if 'commands_before_execution' in desc_data:
        commands_before_execution = [str(command) for command in desc_data['commands_before_execution']]
    if 'commands_after_execution' in desc_data:
        commands_after_execution = [str(command) for command in desc_data['commands_after_execution']]
    if 'variables' in desc_data:
        variables = dict(desc_data['variables'])
    if 'timeout' in desc_data:
        timeout = float(desc_data['timeout'])

    if args.working_directory:
        working_directory = args.working_directory

    if args.output_directory:
        output_directory = args.output_directory

    # Let's update some fields according to the variables we have
    output_directory = evaluate_variables_in_string(output_directory, variables)
    working_directory = evaluate_variables_in_string(working_directory, variables)

    # Let's add some variables
    variables['working_directory'] = working_directory
    variables['output_directory'] = output_directory

    # Let's update commands from the variables we have
    batsim_command = evaluate_variables_in_string(batsim_command, variables)
    sched_command = evaluate_variables_in_string(sched_command, variables)

    # Let's set the working directory
    os.chdir(working_directory)

    logger.info('Working directory: {wd}'.format(wd = os.getcwd()))
    logger.info('Output directory: {od}'.format(od = output_directory))

    # Let the execution be started
    # Commands before instance execution
    for command in commands_before_execution:
        if not execute_command(working_directory = working_directory,
                               command = command,
                               variables = variables):
            sys.exit(1)

    # Instance execution
    if not execute_one_instance(working_directory = working_directory,
                                output_directory = output_directory,
                                batsim_command = batsim_command,
                                sched_command = sched_command,
                                variables = variables,
                                timeout = timeout):
        sys.exit(2)

    # Commands after instance execution
    for command in commands_after_execution:
        if not execute_command(working_directory = working_directory,
                               command = command,
                               variables = variables):
            sys.exit(3)

    # Everything went succesfully, let's return 0
    sys.exit(0)

if __name__ == '__main__':
    main()

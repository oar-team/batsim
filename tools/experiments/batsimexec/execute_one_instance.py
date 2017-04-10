#!/usr/bin/python2

import argparse
from execo import *
import math
import os
import random
import re
import shlex
import stat
import sys
import time
import yaml

class ArgumentParserError(Exception): pass

class ThrowingArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise ArgumentParserError(message)

def find_info_from_batsim_command(batsim_command):
    split_command = shlex.split(batsim_command)

    assert(len(split_command) > 1), "Invalid Batsim command: '{}'".format(
                                    batsim_command)

    if 'batsim' not in split_command[0]:
        logger.warning("Batsim does not seem to be executed directly. "
                       "This may cause this script failing at detecting what "
                       "Batsim's socket is.")

        # Let's assume Batsim's command is just prefixed.
        # Let's find the index at which Batsim's command appears
        found_index = -1

        for i in range(1, len(split_command)):
            if 'batsim' in split_command[i]:
                found_index = i
                break

        if found_index == -1:
            logger.warning("Could not find where Batsim command starts in the "
                           "user-given command '{}'.".format(batsim_command))
        else:
            split_command = split_command[found_index:]

    batparser = ThrowingArgumentParser(prog = split_command[0],
                                       description = 'Batsim command parser',
                                       add_help = False)

    batparser.add_argument("-p", "--platform", type=str)
    batparser.add_argument("-w", "--workload", type=str, nargs='+')
    batparser.add_argument("-W", "--workflow", type=str, nargs='+')
    batparser.add_argument("--WS", "--workflow-start", type=str, nargs='+')

    batparser.add_argument("-e", "--export", type=str, default="out")
    batparser.add_argument("-m", "--master-host", default="master_host")
    batparser.add_argument("-E", "--energy", action='store_true')

    batparser.add_argument("-s", "--socket", type=str, default="/tmp/bat_socket")
    batparser.add_argument("--redis-hostname", type=str, default="127.0.0.1")
    batparser.add_argument("--redis-port", type=int, default=6379)

    batparser.add_argument("--enable-sg-process-tracing", action='store_true')
    batparser.add_argument("--disable-schedule-tracing", action='store_true')
    batparser.add_argument("--disable-machine-state-tracing", action='store_true')

    batparser.add_argument("--mmax", type=int, default=0)
    batparser.add_argument("--mmax-workload", action='store_true')

    batparser.add_argument("-v", "--verbosity", type=str, default="information")
    batparser.add_argument("-q", "--quiet", action='store_true')

    batparser.add_argument("--workflow-jobs-limit", type=int, default=0)
    batparser.add_argument("--ignore-beyond-last-workflow", action='store_true')

    batparser.add_argument("--allow-time-sharing", action='store_true')
    batparser.add_argument("--batexec", action='store_true')
    batparser.add_argument("--pfs-host", type=str, default="pfs_host")

    batparser.add_argument("-h", "--help", action='store_true')
    batparser.add_argument("--version", action='store_true')

    try:
        batargs = batparser.parse_args(split_command[1:])
        is_batexec = False
        if batargs.batexec:
            is_batexec = True

        return (batargs.socket, is_batexec)
    except ArgumentParserError as e:
        logger.error("Could not retrieve Batsim's socket from Batsim's command "
                     "'{}'. Parsing error: '{}'".format(batsim_command, e))
        sys.exit(-1)

def write_string_into_file(string, filename):
    f = open(filename, 'w')
    f.write(string)
    f.close()

def create_dir_if_not_exists(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def delete_file_if_exists(filename):
    if os.path.exists(filename):
        os.remove(filename)

def random_string(length = 16):
    assert(length > 0)
    alphanum="abcdefghijklmnopqrstuvwxyz0123456789"
    s = ''
    for i in range(length):
        s += random.choice(alphanum)
    return s

def make_file_executable(filename):
    st = os.stat(filename)
    os.chmod(filename, st.st_mode | stat.S_IEXEC)

def retrieve_info_from_instance(variables,
                                variables_declaration_order,
                                working_directory,
                                batsim_command):
    filename_ok = False
    while not filename_ok:
        r = random_string()
        script_filename = '{wd}/{rand}_script.sh'.format(wd=working_directory,
                                                         rand=r)
        output_dir_filename = '{wd}/{rand}_out_dir'.format(wd=working_directory,
                                                           rand=r)
        working_dir_filename = '{wd}/{rand}_working_dir'.format(wd=working_directory,
                                                                rand=r)
        command_filename = '{wd}/{rand}_command'.format(wd=working_directory,
                                                               rand=r)
        filename_ok = not os.path.exists(script_filename) and not os.path.exists(output_dir_filename) and not os.path.exists(working_dir_filename) and not os.path.exists(command_filename)

    put_variables_in_file(variables, variables_declaration_order, script_filename)

    # Let's add some directives to prepare the instance!
    text_to_add = "# Preparation\n"
    text_to_add += 'echo {v} > {f}\n'.format(v = "${output_directory}",
                                             f = output_dir_filename)
    text_to_add += 'echo {v} > {f}\n'.format(v = "${working_directory}",
                                             f = working_dir_filename)
    text_to_add += "echo {v} > {f}\n".format(v= batsim_command,
                                             f = command_filename)

    # Let's append the directives in the file
    f = open(script_filename, 'a')
    f.write(text_to_add)
    f.close()

    # Let's execute the script
    p = Process(cmd = 'bash {f}'.format(f=script_filename),
                shell = True,
                name = "Preparation command",
                kill_subprocesses = True,
                cwd = working_directory)

    p.start().wait()
    assert(p.finished_ok and not p.error)

    # Let's get the working directory
    f = open(working_dir_filename, 'r')
    working_dir = f.read().strip()
    f.close()

    # Let's get the output directory
    f = open(output_dir_filename, 'r')
    output_dir = f.read().strip()
    f.close()

    # Let's get the command
    f = open(command_filename, 'r')
    command = f.read().strip()
    f.close()

    # Let's remove temporary files
    delete_file_if_exists(script_filename)

    delete_file_if_exists(working_dir_filename)
    delete_file_if_exists(output_dir_filename)
    delete_file_if_exists(command_filename)

    return (working_dir, output_dir, command)

def find_all_values(var_value):
    ret = set()

    if isinstance(var_value, dict):
        for subvalue in var_value.values():
            ret.update(find_all_values(subvalue))
    elif isinstance(var_value, list):
        for subvalue in var_value:
            ret.update(subvalue)
    else:
        ret.add(str(var_value))
    return ret

def check_variables(variables):
    # Let's check that they have valid bash identifier names
    for var_name in variables:
        if not is_valid_identifier(var_name):
            logger.error("Invalid variable name '{var_name}'".format(var_name))
            return (False, [])

    # Let's check whether the dependency graph is a DAG
    # Let's initialize dependencies
    dependencies = {}
    for var_name in variables:
        dependencies[var_name] = set()

    # Let's compute the dependencies of each variable
    for var_name in variables:
        in_depth_values = find_all_values(variables[var_name])
        logger.debug("in_depth_values of {}: {}".format(var_name, in_depth_values))
        for potential_dependency_name in variables:
            substr = '${' + potential_dependency_name
            for depth_value in in_depth_values:
                if substr in depth_value:
                    dependencies[var_name].add(potential_dependency_name)

    # Let's sort the variables by ascending number of dependencies
    ordered = [(len(dependencies[var_name]), var_name, dependencies[var_name]) for var_name in dependencies]
    ordered.sort()

    # Let's do the DAG check while building a declaration order
    declared_variables = set()
    variables_declaration_order = []
    saturated = False

    while len(declared_variables) < len(variables) and not saturated:
        saturated = True
        for (nb_deps, var_name, deps) in ordered:
            all_deps_declared = True
            for dep_name in deps:
                if dep_name not in declared_variables:
                    all_deps_declared = False
            if all_deps_declared:
                declared_variables.add(var_name)
                variables_declaration_order.append(var_name)
                saturated = False
                ordered.remove((nb_deps, var_name, deps))
                logger.debug("Declared {}, declared_vars:{}".format(var_name, declared_variables))
                break

    if len(declared_variables) == len(variables):
        return (True, variables_declaration_order)
    else:
        undeclared_variables = set([var_name for var_name in variables]) - declared_variables
        undeclared_variables = list(undeclared_variables)
        logger.error("Invalid variables: Could not find a declaration order that allows to declare these variables: {}. All variables : {}".format(undeclared_variables, variables))
        return (False, [])

def is_valid_identifier(string):
    return re.match("^[_A-Za-z][_a-zA-Z0-9]*$", string)

def variable_to_text(variables, var_name):
    text = ""
    var_value = variables[var_name]
    if isinstance(var_value, tuple) or isinstance(var_value, list):
        text += "declare -a {var_name}\n".format(var_name = var_name)
        for element_id in range(len(var_value)):
            text += '{var_name}["{id}"]="{value}"\n'.format(
                var_name = var_name,
                id = element_id,
                value = var_value[element_id])
        text += '\n'
    elif isinstance(var_value, dict):
        text += "declare -A {var_name}\n".format(var_name = var_name)
        for key in var_value:
            text += '{var_name}["{key}"]="{value}"\n'.format(
                var_name = var_name,
                key = key,
                value = var_value[key])
        text += '\n'
    else:
        text += '{var_name}="{value}"\n'.format(var_name = var_name,
                                                         value = var_value)

    return text

def put_variables_in_file(variables,
                          variables_declaration_order,
                          output_filename):
    text_to_write = "#!/bin/bash\n"

    # Let's define all variables in the specified order
    text_to_write += "\n# Variables definition\n"

    for var_name in variables_declaration_order:
        assert(var_name in variables)
        text_to_write += variable_to_text(variables, var_name)

    # Let's export all variables
    text_to_write +="\n# Export variables\n"
    for var_name in variables_declaration_order:
        text_to_write += "export {var_name}\n".format(var_name = var_name)

    text_to_write += "\n"
    write_string_into_file(text_to_write, output_filename)

def create_file_from_command(command,
                             output_filename,
                             variables_definition_filename):
    text_to_write = "#!/bin/bash\n"

    # Let's define all variables
    text_to_write += "\n# Variables definition\n"
    text_to_write += "source {}".format(variables_definition_filename)

    # Let's write the commands
    text_to_write += "\n\n# User-specified commands\n"
    text_to_write += command
    text_to_write += "\n"

    write_string_into_file(text_to_write, output_filename)

    # Let's chmod +x the script
    make_file_executable(output_filename)

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
        self.execution_data.nb_started += 1

    def end(self, process):
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

            for (stream, sname) in [(process.stdout, 'stdout'),
                                    (process.stderr, 'stderr')]:
                if stream != '':
                    max_nb_lines = 10
                    lines = stream.split('\n')
                    head = '\n'
                    if len(lines) > max_nb_lines:
                        head = '[... only the last {} lines are shown]\n'.format(max_nb_lines)
                    logger.info('Batsim {s}:\n{h}{l}'.format(s = sname,
                                                             h = head,
                                                             l = '\n'.join(lines)))

            if self.execution_data.sched_process.running:
                logger.warning("Killing Sched")
                self.execution_data.sched_process.kill(auto_force_kill_timeout = 30)
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

            for (stream, sname) in [(process.stdout, 'stdout'),
                                    (process.stderr, 'stderr')]:
                if stream != '':
                    max_nb_lines = 10
                    lines = stream.split('\n')
                    head = '\n'
                    if len(lines) > max_nb_lines:
                        head = '[... only the last {} lines are shown]\n'.format(max_nb_lines)
                    logger.info('Sched {s}:\n{h}{l}'.format(s = sname,
                                                            h = head,
                                                            l = '\n'.join(lines)))

            if self.execution_data.batsim_process.running:
                logger.warning("Killing Batsim")
                self.execution_data.batsim_process.kill(auto_force_kill_timeout = 30)
        else:
            logger.info("Sched ended successfully")
        self.execution_data.nb_finished += 1

def execute_command(command,
                    working_directory,
                    variables_filename,
                    output_script_filename,
                    output_subscript_filename,
                    output_script_output_dir,
                    command_name):

    # If the command is composed of different lines,
    # a corresponding script file will be created
    if '\n' in command:
        multiline_command = True
        write_string_into_file(command, output_subscript_filename)
        make_file_executable(output_subscript_filename)

        fake_command = os.path.abspath(output_subscript_filename)
        create_file_from_command(fake_command,
                                 output_filename = output_script_filename,
                                 variables_definition_filename = variables_filename)
    else:
        create_file_from_command(command = command,
                                 output_filename = output_script_filename,
                                 variables_definition_filename = variables_filename)

    # Let's create the execo process
    cmd_process = Process(cmd = 'bash {f}'.format(f=output_script_filename),
                          shell = True,
                          kill_subprocesses = True,
                          name = command_name,
                          cwd = working_directory)

    logger.info("Executing command: {cmd}".format(cmd=command))

    # Let's start the process
    cmd_process.start().wait()

    # Let's write command outputs
    create_dir_if_not_exists(output_script_output_dir)
    write_string_into_file(cmd_process.stdout, '{out}/{name}.stdout'.format(
                            out = output_script_output_dir,
                            name = command_name))
    write_string_into_file(cmd_process.stderr, '{out}/{name}.stderr'.format(
                            out = output_script_output_dir,
                            name = command_name))

    return cmd_process.finished_ok and not cmd_process.error and cmd_process.exit_code == 0

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
        #logger.debug("Batsim stderr: {}".format(execution_data.batsim_process.stderr))

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
                         variables_filename,
                         timeout = None,
                         do_not_execute = False):
    if timeout == None:
        timeout = 3600

    logger.info('Batsim command: "{}"'.format(batsim_command))
    logger.info('Sched command: "{}"'.format(sched_command))

    # Let's create the output directory if it does not exist
    create_dir_if_not_exists(output_directory)

    # Let's wrap the two commands into files
    batsim_script_filename = '{output_dir}/batsim_command.sh'.format(
        output_dir = output_directory)
    sched_script_filename = '{output_dir}/sched_command.sh'.format(
        output_dir = output_directory)

    create_file_from_command(command = batsim_command,
                             output_filename = batsim_script_filename,
                             variables_definition_filename = variables_filename)

    create_file_from_command(command = sched_command,
                             output_filename = sched_script_filename,
                             variables_definition_filename = variables_filename)

    if do_not_execute == True:
        logger.info('Skipping the execution of the instance because '
                    'do_not_execute is True.')
        sys.exit(4)

    # Let's create lifecycle handlers, which will manage what to do on process's events
    batsim_lifecycle_handler = BatsimLifecycleHandler()
    sched_lifecycle_handler = SchedLifecycleHandler()

    # If batsim's socket is still opened, let's wait for it to close
    (batsim_socket, is_batexec) = find_info_from_batsim_command(batsim_command)
    socket_usable = wait_for_batsim_socket_to_be_usable(sock = batsim_socket,
                                                        timeout = timeout)
    assert(socket_usable)
    logger.info("Socket {sock} is now usable".format(sock = batsim_socket))

    # Let's create the execo processes
    batsim_process = Process(cmd = 'bash {batsim_script}'.format(
                                batsim_script = batsim_script_filename),
                             shell = True,
                             kill_subprocesses = True,
                             name = "batsim_process",
                             cwd = working_directory,
                             timeout = timeout,
                             lifecycle_handlers = [batsim_lifecycle_handler])

    sched_process = Process(cmd = 'bash {sched_script}'.format(
                                sched_script = sched_script_filename),
                            shell = True,
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

    # Wait for Batsim to create the socket
    logger.info("Waiting for socket {} to open".format(execution_data.batsim_socket))
    if wait_for_batsim_to_open_connection(execution_data,
                                          timeout = execution_data.timeout,
                                          sock = execution_data.batsim_socket):
        # Launches the scheduler
        logger.info("Batsim's socket {} is now opened".format(execution_data.batsim_socket))
        execution_data.sched_process.start()

        # Wait for processes' termination
        while (execution_data.failure == False) and (execution_data.nb_finished < 2):
            sleep(0.2)

        while (execution_data.nb_finished < execution_data.nb_started):
            sleep(0.2)

    elif is_batexec:
        success = True
    else:
        execution_data.failure = True

    if execution_data != None:
        success = not execution_data.failure
    else:
        success = True

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

    g = p.add_mutually_exclusive_group()

    g.add_argument('--post_only',
                   action = 'store_true',
                   help = 'If set, only the post commands of this instance will '
                          'be computed.')

    g.add_argument('--do_not_execute',
                   action = 'store_true',
                   help = 'If set, the Batsim+scheduler instance is not '
                          'executed, only the precommands are. The execution '
                          'scripts should also be generated. Furthermore, post '
                          'commands will be skipped.')

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

    do_not_execute = False
    if args.do_not_execute:
        do_not_execute = True


    # Let's add some variables
    variables['working_directory'] = working_directory
    variables['output_directory'] = output_directory

     # Let's check that variables are fine
    (var_ok, var_decl_order) = check_variables(variables)
    if not var_ok:
        sys.exit(-2)

    # Let's correctly interpret the working_dir and output_dir values
    (wd, od, batsim_command) = retrieve_info_from_instance(variables,
                                                           var_decl_order,
                                                           "/tmp",
                                                           batsim_command)

    # Let's update those values
    variables['working_directory'] = working_directory = wd
    variables['output_directory'] = output_directory = od

    print("Variables = {}".format(variables))

    # Let's create the directories if needed
    create_dir_if_not_exists(working_directory)
    create_dir_if_not_exists(output_directory)

    # Let's set the working directory
    os.chdir(working_directory)

    logger.info('Working directory: {wd}'.format(wd = os.getcwd()))
    logger.info('Output directory: {od}'.format(od = output_directory))

    # Let's create a variable definition file in the instance output directory
    variables_filename = '{out}/variables.bash'.format(out = output_directory)
    put_variables_in_file(variables, var_decl_order, variables_filename)

    if not args.post_only:
        # Let the execution be started
        # Commands before instance execution
        if len(commands_before_execution) > 0:
            pre_commands_dir = '{instance_out_dir}/pre_commands'.format(
                instance_out_dir = output_directory)
            pre_commands_output_dir = '{commands_dir}/out'.format(
                commands_dir = pre_commands_dir)
            create_dir_if_not_exists(pre_commands_dir)
            create_dir_if_not_exists(pre_commands_output_dir)

            nb_chars_command_ids = int(1 + math.log10(len(commands_before_execution)))

            for command_id in range(len(commands_before_execution)):
                command_name = 'command' + str(command_id).zfill(nb_chars_command_ids)
                output_command_filename = '{commands_dir}/{name}.bash'.format(
                                            commands_dir = pre_commands_dir,
                                            name = command_name)
                output_subscript_filename = '{commands_dir}/{name}_sub'.format(
                                                commands_dir = pre_commands_dir,
                                                name = command_name)

                if not execute_command(command = commands_before_execution[command_id],
                                       working_directory = working_directory,
                                       variables_filename = variables_filename,
                                       output_script_filename = output_command_filename,
                                       output_subscript_filename = output_subscript_filename,
                                       output_script_output_dir = pre_commands_output_dir,
                                       command_name = command_name):
                    sys.exit(1)

        # Instance execution
        if not execute_one_instance(working_directory = working_directory,
                                    output_directory = output_directory,
                                    batsim_command = batsim_command,
                                    sched_command = sched_command,
                                    variables_filename = variables_filename,
                                    timeout = timeout,
                                    do_not_execute = do_not_execute):
            sys.exit(2)

    # Commands after instance execution
    if len(commands_after_execution) > 0:
        post_commands_dir = '{instance_out_dir}/post_commands'.format(
            instance_out_dir = output_directory)
        post_commands_output_dir = '{commands_dir}/out'.format(
            commands_dir = post_commands_dir)
        create_dir_if_not_exists(post_commands_dir)
        create_dir_if_not_exists(post_commands_output_dir)

        nb_chars_command_ids = int(1 + math.log10(len(commands_after_execution)))

        for command_id in range(len(commands_after_execution)):
            command_name = 'command' + str(command_id).zfill(nb_chars_command_ids)
            output_command_filename = '{commands_dir}/{name}.bash'.format(
                                        commands_dir = post_commands_dir,
                                        name = command_name)
            output_subscript_filename = '{commands_dir}/{name}_sub'.format(
                                            commands_dir = post_commands_dir,
                                            name = command_name)

            if not execute_command(command = commands_after_execution[command_id],
                                   working_directory = working_directory,
                                   variables_filename = variables_filename,
                                   output_script_filename = output_command_filename,
                                   output_subscript_filename = output_subscript_filename,
                                   output_script_output_dir = post_commands_output_dir,
                                   command_name = command_name):
                sys.exit(3)

    # Everything went succesfully, let's return 0
    sys.exit(0)

if __name__ == '__main__':
    main()

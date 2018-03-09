#!/usr/bin/env python3

"""Execute one Batsim instance."""

import argparse
import asyncio.subprocess
import async_timeout
import coloredlogs
import logging
import math
import os
import random
import re
import shlex
import stat
import subprocess
import sys
import time
import yaml

logger = logging.getLogger('execute_one_instance')
coloredlogs.install(milliseconds=True)


class ArgumentParserError(Exception):
    """A custom Exception type when parsing with argparse."""

    pass


class ThrowingArgumentParser(argparse.ArgumentParser):
    """An ArgumentParser that throws exceptions on errors."""

    def error(self, message):
        """Callback function called when parsing errors arise."""
        raise ArgumentParserError(message)


def find_info_from_batsim_command(batsim_command):
    """Get information from the Batsim command."""
    split_command = shlex.split(batsim_command)

    assert len(split_command) > 1, "Invalid Batsim command: '{}'".format(
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

    batparser = ThrowingArgumentParser(prog=split_command[0],
                                       description='Batsim command parser',
                                       add_help=False)

    batparser.add_argument("-p", "--platform", type=str)
    batparser.add_argument("-w", "--workload", type=str, nargs='+')
    batparser.add_argument("-W", "--workflow", type=str, nargs='+')
    batparser.add_argument("--WS", "--workflow-start", type=str, nargs='+')

    batparser.add_argument("-e", "--export", type=str, default="out")
    batparser.add_argument("-m", "--master-host", default="master_host")
    batparser.add_argument("-E", "--energy", action='store_true')
    batparser.add_argument('--add-role-to-hosts', type=str, default="None")

    batparser.add_argument('--config-file', type=str, default="None")
    batparser.add_argument("-s", "--socket-endpoint",
                           type=str, default="tcp://localhost:28000")
    batparser.add_argument("--redis-hostname", type=str, default="127.0.0.1")
    batparser.add_argument("--redis-port", type=int, default=6379)
    batparser.add_argument("--redis-prefix", type=str, default='default')

    batparser.add_argument("--enable-sg-process-tracing", action='store_true')
    batparser.add_argument("--disable-schedule-tracing", action='store_true')
    batparser.add_argument(
        "--disable-machine-state-tracing", action='store_true')

    batparser.add_argument("--mmax", type=int, default=0)
    batparser.add_argument("--mmax-workload", action='store_true')

    batparser.add_argument("-v", "--verbosity",
                           type=str, default="information")
    batparser.add_argument("-q", "--quiet", action='store_true')

    batparser.add_argument("--workflow-jobs-limit", type=int, default=0)
    batparser.add_argument(
        "--ignore-beyond-last-workflow", action='store_true')

    batparser.add_argument("--allow-time-sharing", action='store_true')
    batparser.add_argument("--batexec", action='store_true')

    batparser.add_argument("-h", "--help", action='store_true')
    batparser.add_argument("--version", action='store_true')

    try:
        batargs = batparser.parse_args(split_command[1:])
        is_batexec = False
        if batargs.batexec:
            is_batexec = True

        return (batargs.socket_endpoint, is_batexec)
    except ArgumentParserError as e:
        logger.error("Could not retrieve Batsim's socket from Batsim's command"
                     " '{}'. Parsing error: '{}'".format(batsim_command, e))
        sys.exit(-1)


def write_string_into_file(string, filename):
    """Write a string into a file."""
    f = open(filename, 'w')
    f.write(string)
    f.close()


def create_dir_if_not_exists(directory):
    """Python mkdir -p."""
    if not os.path.exists(directory):
        os.makedirs(directory)


def delete_file_if_exists(filename):
    """Only deletes a file if it exists."""
    if os.path.exists(filename):
        os.remove(filename)


def random_string(length=16):
    """Generate a alphanum string."""
    assert(length > 0)
    alphanum = "abcdefghijklmnopqrstuvwxyz0123456789"
    s = ''
    for i in range(length):
        s += random.choice(alphanum)
    return s


def make_file_executable(filename):
    """Python chmod +x."""
    st = os.stat(filename)
    os.chmod(filename, st.st_mode | stat.S_IEXEC)


def retrieve_info_from_instance(variables,
                                variables_declaration_order,
                                working_directory,
                                batsim_command):
    """Retrieve information from the instance."""
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
        filename_ok = not os.path.exists(script_filename) and not os.path.exists(
            output_dir_filename) and not os.path.exists(working_dir_filename) and not os.path.exists(command_filename)

    put_variables_in_file(
        variables, variables_declaration_order, script_filename)

    # Let's add some directives to prepare the instance!
    text_to_add = "# Preparation\n"
    text_to_add += 'echo {v} > {f}\n'.format(v="${output_directory}",
                                             f=output_dir_filename)
    text_to_add += 'echo {v} > {f}\n'.format(v="${working_directory}",
                                             f=working_dir_filename)
    text_to_add += "echo {v} > {f}\n".format(v=batsim_command,
                                             f=command_filename)

    # Let's append the directives in the file
    f = open(script_filename, 'a')
    f.write(text_to_add)
    f.close()

    # Let's execute the script
    p = subprocess.run('bash {f}'.format(f=script_filename),
                       shell=True, stdout=subprocess.PIPE)
    assert(p.returncode == 0)

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
    """Check whether variables are valid."""
    # Let's check that they have valid bash identifier names
    for var_name in variables:
        if not is_valid_identifier(var_name):
            logger.error("Invalid variable name '{}'".format(var_name))
            return (False, [])

    # Let's check whether the dependency graph is a DAG
    # Let's initialize dependencies
    dependencies = {}
    for var_name in variables:
        dependencies[var_name] = set()

    # Let's compute the dependencies of each variable
    for var_name in variables:
        in_depth_values = find_all_values(variables[var_name])
        logger.debug("in_depth_values of {}: {}".format(
            var_name, in_depth_values))
        for potential_dependency_name in variables:
            r = re.compile('.*\${' + potential_dependency_name + '[\[}].*')
            for depth_value in in_depth_values:
                if r.match(depth_value):
                    dependencies[var_name].add(potential_dependency_name)

    logger.debug('Dependencies : {}'.format(dependencies))

    # Let's sort the variables by ascending number of dependencies
    ordered = [(len(dependencies[var_name]), var_name, dependencies[var_name])
               for var_name in dependencies]
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
                logger.debug("Declared {}, declared_vars:{}".format(
                    var_name, declared_variables))
                break

    if len(declared_variables) == len(variables):
        return (True, variables_declaration_order)
    else:
        undeclared_variables = set(
            [var_name for var_name in variables]) - declared_variables
        undeclared_variables = list(undeclared_variables)
        logger.error("Invalid variables: Could not find a declaration order that allows to declare these variables: {}. All variables : {}".format(
            undeclared_variables, variables))
        return (False, [])


def is_valid_identifier(string):
    """Return whether a given string is a valid (variable) identifier."""
    return re.match("^[_A-Za-z][_a-zA-Z0-9]*$", string)


def variable_to_text(variables, var_name):
    """Generate the bash declaration of a variable."""
    text = ""
    var_value = variables[var_name]
    if isinstance(var_value, tuple) or isinstance(var_value, list):
        text += "declare -a {var_name}\n".format(var_name=var_name)
        for element_id in range(len(var_value)):
            text += '{var_name}["{id}"]="{value}"\n'.format(
                var_name=var_name,
                id=element_id,
                value=var_value[element_id])
        text += '\n'
    elif isinstance(var_value, dict):
        text += "declare -A {var_name}\n".format(var_name=var_name)
        for key in var_value:
            text += '{var_name}["{key}"]="{value}"\n'.format(
                var_name=var_name,
                key=key,
                value=var_value[key])
        text += '\n'
    else:
        text += '{var_name}="{value}"\n'.format(var_name=var_name,
                                                value=var_value)

    return text


def put_variables_in_file(variables,
                          variables_declaration_order,
                          output_filename):
    """Put bash declarations of variables into a file."""
    text_to_write = "#!/usr/bin/env bash\n"

    # Let's define all variables in the specified order
    text_to_write += "\n# Variables definition\n"

    for var_name in variables_declaration_order:
        assert(var_name in variables)
        text_to_write += variable_to_text(variables, var_name)

    # Let's export all variables
    text_to_write += "\n# Export variables\n"
    for var_name in variables_declaration_order:
        text_to_write += "export {var_name}\n".format(var_name=var_name)

    text_to_write += "\n"
    write_string_into_file(text_to_write, output_filename)


def create_file_from_command(command,
                             output_filename,
                             variables_definition_filename):
    """Generate a bash script from a string command."""
    text_to_write = "#!/usr/bin/env bash\n"

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


def display_process_output_on_error(process_name, stdout_file, stderr_file,
                                    max_lines=42):
    """Display process output when on error."""
    if (stdout_file is None) and (stderr_file is None):
        logger.error("Cannot retrieve any log about the failed process")
        return

    for filename, fname in [(stdout_file.name, "stdout"),
                            (stderr_file.name, "stderr")]:
        if filename is not None:
            # Let's retrieve the file length (ugly.)
            cmd_wc = "wc -l {}".format(filename)
            p = subprocess.run(cmd_wc, shell=True, stdout=subprocess.PIPE)
            assert(p.returncode == 0)
            nb_lines = int(str(p.stdout.decode('utf-8')).split(' ')[0])

            if nb_lines > 0:
                # Let's read the whole file if it is small
                if nb_lines <= max_lines:
                    with open(filename, 'r') as f:
                        logger.error('{} {}:\n{}'.format(process_name, fname,
                                                         f.read()))
                # Let's read the first and last lines of the file
                else:
                    cmd_head = "head -n {} {}".format(max_lines // 2, filename)
                    cmd_tail = "tail -n {} {}".format(max_lines // 2, filename)

                    p_head = subprocess.run(cmd_head, shell=True,
                                            stdout=subprocess.PIPE)
                    p_tail = subprocess.run(cmd_tail, shell=True,
                                            stdout=subprocess.PIPE)

                    assert(p_head.returncode == 0)
                    assert(p_tail.returncode == 0)

                    logger.error('{} {}:\n{}\n...\n...\n... (truncated... whole log in {})\n...\n...\n{}'.format(
                        process_name,
                        fname, str(p_head.stdout.decode('utf-8')), filename,
                        str(p_tail.stdout.decode('utf-8'))))

async def execute_command_inner(command, stdout_file=None, stderr_file=None,
                                timeout=None, process_name=None,
                                where_to_append_proc=None):
    """Execute one command asynchronously."""
    create = asyncio.create_subprocess_shell(command,
                                             stdout=stdout_file,
                                             stderr=stderr_file)
    # Wait for the subprocess creation
    proc = await create

    if where_to_append_proc is not None:
        where_to_append_proc.add(proc)

    with async_timeout.timeout(timeout):
        # Wait for subprocess termination
        await proc.wait()

    return proc, process_name


async def run_wait_any(batsim_command, sched_command,
                       batsim_stdout_file, batsim_stderr_file,
                       sched_stdout_file, sched_stderr_file,
                       timeout=None, where_to_append_proc=None):
    """Asynchronously waits for the termination of Batsim or Sched."""
    done, pending = await asyncio.wait([execute_command_inner(batsim_command,
                                                              batsim_stdout_file, batsim_stderr_file,
                                                              timeout, "Batsim", where_to_append_proc),
                                        execute_command_inner(sched_command,
                                                              sched_stdout_file, sched_stderr_file,
                                                              timeout, "Sched", where_to_append_proc)],
                                       return_when=asyncio.FIRST_COMPLETED)
    return done, pending


async def await_with_timeout(future, timeout=None):
    """Await wrapper, which handles a timeout."""
    with async_timeout.timeout(timeout):
        await future
        return future


def kill_processes_and_all_descendants(proc_set):
    """Kill a set of processes and all their children."""
    pids_to_kill = set()
    for proc in proc_set:
        cmd = "pstree {} -pal | cut -d',' -f2 | cut -d' ' -f1 | cut -d')' -f1".format(
            proc.pid)
        p = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE)
        for pid in [int(pid_str) for pid_str in p.stdout.decode('utf-8').replace('\n', ' ').strip().split(' ')]:
            pids_to_kill.add(pid)

    assert(os.getpid() not in pids_to_kill)
    logger.error("Killing remaining processes {}".format(pids_to_kill))
    cmd = "kill -9 {}".format(' '.join([str(pid) for pid in pids_to_kill]))
    p = subprocess.run(cmd, shell=True)


def execute_batsim_alone(batsim_command, batsim_stdout_file, batsim_stderr_file,
                         timeout=None):
    """Execute Batsim and wait for its termination."""
    loop = asyncio.get_event_loop()
    proc_set = set()
    try:
        out_files = {'Batsim': (batsim_stdout_file, batsim_stderr_file)}
        logger.info("Running Batsim")
        proc, pname = loop.run_until_complete(execute_command_inner(
            batsim_command, batsim_stdout_file,
            batsim_stderr_file, timeout, "Batsim",
            proc_set))
        proc_set.remove(proc)
        if proc.returncode == 0:
            logger.info("{} finished".format(pname))
            return True
        else:
            logger.error("{} finished (returncode={})".format(pname,
                                                              proc.returncode))
            display_process_output_on_error(pname, *out_files[pname])
            return False
    except asyncio.TimeoutError:
        logger.error("Timeout reached!")
    except Exception as e:
        logger.error('Exception caught ({}): {}'.format(type(e), e))
    except:
        logger.error("Another exception caught!")

    if len(proc_set) > 0:
        kill_processes_and_all_descendants(proc_set)

    return False


def execute_batsim_and_sched(batsim_command, sched_command,
                             batsim_stdout_file, batsim_stderr_file,
                             sched_stdout_file, sched_stderr_file,
                             timeout=None, wait_timeout_on_success=600):
    """Execute Batsim+Sched and wait for their termination."""
    loop = asyncio.get_event_loop()
    proc_set = set()
    try:
        out_files = {'Batsim': (batsim_stdout_file, batsim_stderr_file),
                     'Sched': (sched_stdout_file, sched_stderr_file)}
        logger.info("Running Batsim and Sched")
        done, pending = loop.run_until_complete(run_wait_any(
            batsim_command, sched_command,
            batsim_stdout_file, batsim_stderr_file,
            sched_stdout_file, sched_stderr_file,
            timeout, proc_set))
        finished_tuples = [finished.result() for finished in done]
        assert(len(finished_tuples) in {1, 2})

        if (len(done) == 2):
            logger.info("Both processes finished at the same time!")
            all_ok = True
            for finished_tuple in finished_tuples:
                if finished_tuple[0].returncode != 0:
                    all_ok = False
                    logger.error("{} finished (returncode={})".format(
                        finished_tuple[1],
                        finished_tuple[0].returncode))
                    display_process_output_on_error(finished_tuple[1],
                                                    *out_files[finished_tuple[1]])
                else:
                    logger.info("{} finished".format(finished_tuple[1]))
            return all_ok

        # One (and only one) process finished.
        finished_proc, finished_pname = finished_tuples[0]
        finished_ok = finished_proc.returncode == 0
        proc_set.remove(finished_proc)

        if finished_ok:
            logger.info("{} finished".format(finished_pname))
            logger.info("Waiting for clean termination... (timeout={})".format(
                wait_timeout_on_success))

            pending_future = next(iter(pending))
            finished = loop.run_until_complete(await_with_timeout(
                pending_future, wait_timeout_on_success))
            finished_proc, finished_pname = finished.result()
            proc_set.remove(finished_proc)

            if finished_proc.returncode == 0:
                logger.info("{} finished".format(finished_pname))
                return True
            else:
                logger.error("{} finished (returncode={})".format(
                    finished_pname, finished_proc.returncode))
                display_process_output_on_error(finished_pname,
                                                *out_files[finished_pname])
                return False
        else:
            logger.error("{} finished (returncode={})".format(
                finished_pname, finished_proc.returncode))
            display_process_output_on_error(finished_pname,
                                            *out_files[finished_pname])
    except asyncio.TimeoutError:
        logger.error("Timeout reached!")
    except Exception as e:
        logger.error('Exception caught ({}): {}'.format(type(e), e))
    except:
        logger.error("Another exception caught!")

    if len(proc_set) > 0:
        kill_processes_and_all_descendants(proc_set)

    return False


def execute_command(command,
                    working_directory,
                    variables_filename,
                    output_script_filename,
                    output_subscript_filename,
                    output_script_output_dir,
                    command_name,
                    timeout=None):
    """Execute a command synchronously."""
    # If the command is composed of different lines,
    # a corresponding subscript file will be created.
    # Otherwise, only one script will be created.
    if '\n' in command:
        write_string_into_file(command, output_subscript_filename)
        make_file_executable(output_subscript_filename)

        fake_command = os.path.abspath(output_subscript_filename)
        create_file_from_command(fake_command,
                                 output_filename=output_script_filename,
                                 variables_definition_filename=variables_filename)
    else:
        create_file_from_command(command=command,
                                 output_filename=output_script_filename,
                                 variables_definition_filename=variables_filename)

    create_dir_if_not_exists(output_script_output_dir)

    # Let's prepare the real command call
    cmd = 'bash {f}'.format(f=output_script_filename)
    stdout_file = open('{out}/{name}.stdout'.format(out=output_script_output_dir,
                                                    name=command_name), 'wb')
    stderr_file = open('{out}/{name}.stderr'.format(out=output_script_output_dir,
                                                    name=command_name), 'wb')

    # Let's run the command
    logger.info("Executing command '{}'".format(command_name))
    p = subprocess.run(cmd, shell=True, stdout=stdout_file, stderr=stderr_file)

    if p.returncode == 0:
        logger.info("{} finished".format(command_name))
        return True
    else:
        logger.error("Command '{name}' failed.\n--- begin of {name} ---\n"
                     "{content}\n--- end of {name} ---".format(
                         name=command_name, content=command))
        display_process_output_on_error(command_name, stdout_file, stderr_file)
        return False

g_port_regex = re.compile('.*:(\d+)')


def socket_in_use(sock):
    """Return whether the given socket is being used."""
    # Let's check whether the socket uses a port
    m = g_port_regex.match(sock)
    if m:
        port = int(m.group(1))
        cmd = "ss -ln | grep ':{port}'".format(port=port)

        p = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE)
        return len(p.stdout.decode('utf-8')) > 0

    return False


def wait_for_batsim_socket_to_be_usable(sock='tcp://localhost:28000',
                                        timeout=5,
                                        seconds_to_sleep=0.1):
    """Wait some time until the socket is available."""
    logger.info("Waiting for socket '{}' to be usable".format(sock))
    if timeout is None:
        timeout = 5
    remaining_time = timeout
    while remaining_time > 0 and socket_in_use(sock):
        time.sleep(seconds_to_sleep)
        remaining_time -= seconds_to_sleep
    return remaining_time > 0


def execute_one_instance(working_directory,
                         output_directory,
                         batsim_command,
                         interpreted_batsim_command,
                         sched_command,
                         variables_filename,
                         timeout=None,
                         do_not_execute=False):
    """Execute one instance: Batsim(+Sched)."""
    if timeout is None:
        timeout = 604800

    # Let's create the output directory if it does not exist
    create_dir_if_not_exists(output_directory)

    (batsim_socket, is_batexec) = find_info_from_batsim_command(
        interpreted_batsim_command)

    # Let's wrap the two commands into files
    batsim_script_filename = '{output_dir}/batsim_command.sh'.format(
        output_dir=output_directory)
    sched_script_filename = '{output_dir}/sched_command.sh'.format(
        output_dir=output_directory)

    logger.info('Batsim command: "{}"'.format(batsim_command))
    create_file_from_command(command=batsim_command,
                             output_filename=batsim_script_filename,
                             variables_definition_filename=variables_filename)

    if not is_batexec:
        logger.info('Sched command: "{}"'.format(sched_command))
        create_file_from_command(command=sched_command,
                                 output_filename=sched_script_filename,
                                 variables_definition_filename=variables_filename)

    if do_not_execute is True:
        logger.info('Skipping the execution of the instance because '
                    'do_not_execute is True.')
        sys.exit(4)

    # Let's prepare the commands execution
    batsim_command = 'bash {script}'.format(script=batsim_script_filename)
    sched_command = 'bash {script}'.format(script=sched_script_filename)

    batsim_stdout_filename = '{out}/batsim.stdout'.format(out=output_directory)
    batsim_stderr_filename = '{out}/batsim.stderr'.format(out=output_directory)
    sched_stdout_filename = '{out}/sched.stdout'.format(out=output_directory)
    sched_stderr_filename = '{out}/sched.stderr'.format(out=output_directory)

    batsim_stdout_file = open(batsim_stdout_filename, 'wb')
    batsim_stderr_file = open(batsim_stderr_filename, 'wb')

    if not is_batexec:
        sched_stdout_file = open(sched_stdout_filename, 'wb')
        sched_stderr_file = open(sched_stderr_filename, 'wb')

        # If batsim's socket is still opened, let's wait for it to close
        socket_wait_timeout = 5
        socket_usable = wait_for_batsim_socket_to_be_usable(sock=batsim_socket,
                                                            timeout=socket_wait_timeout)
        if not socket_usable:
            logger.error("Socket {} is still busy (after timeout={})".format(
                batsim_socket, socket_wait_timeout))
            return False

        logger.info("Socket {sock} is now usable".format(sock=batsim_socket))
        return execute_batsim_and_sched(batsim_command, sched_command,
                                        batsim_stdout_file, batsim_stderr_file,
                                        sched_stdout_file, sched_stderr_file,
                                        timeout)
    else:
        return execute_batsim_alone(batsim_command, batsim_stdout_file,
                                    batsim_stderr_file, timeout)


def main():
    """
    Script entry point.

    Essentially:
    1. Parse input arguments
    2. (Execute pre-commands)
    3. Execute Batsim(+Sched)
    4. (Execute post-commands)
    """
    script_description = '''
Lauches one Batsim instance.
An instance can be represented by a tuple (platform, workload, algorithm).
It is described in a YAML file, which is the parameter of this script.

Examples of such input files can be found in subdirectory instance_examples.
'''

    # Program parameters parsing
    p = argparse.ArgumentParser(description=script_description)

    p.add_argument('instance_description_filename',
                   type=str,
                   help='The name of the YAML file describing the instance.'
                   'Beware, this argument is not subjected to the working '
                   'directory parameter.')

    p.add_argument('-wd', '--working_directory',
                   type=str,
                   default=None,
                   help='If set, the instance will be executed in the given '
                          'directory. This value has higher priority than the '
                          'one that might be given in the description file. '
                          'If unset, the script working directory is used '
                          'instead')

    p.add_argument('-od', '--output_directory',
                   type=str,
                   default=None,
                   help='If set, the outputs of the current script will be '
                          'put into the given directory. This value has higher'
                          ' priority than the one that might be given in the '
                          'description file. If a value is set, it might be '
                          'either absolute or relative to the working '
                          'directory.'
                          ' If unset, the working directory is used instead')

    g = p.add_mutually_exclusive_group()

    g.add_argument('--post_only',
                   action='store_true',
                   help='If set, only the post commands of this instance will '
                   'be computed.')

    g.add_argument('--do_not_execute',
                   action='store_true',
                   help='If set, the Batsim+scheduler instance is not '
                   'executed, only the precommands are. The execution '
                          'scripts should also be generated. Furthermore, post'
                          ' commands will be skipped.')

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

    assert 'batsim_command' in desc_data
    assert 'sched_command' in desc_data

    batsim_command = str(desc_data['batsim_command'])
    sched_command = str(desc_data['sched_command'])

    if 'working_directory' in desc_data:
        working_directory = str(desc_data['working_directory'])
    if 'output_directory' in desc_data:
        output_directory = str(desc_data['output_directory'])
    if 'commands_before_execution' in desc_data:
        commands_before_execution = [str(command) for command in desc_data[
            'commands_before_execution']]
    if 'commands_after_execution' in desc_data:
        commands_after_execution = [str(command) for command in desc_data[
            'commands_after_execution']]
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
    (wd, od, interpreted_batsim_command) = retrieve_info_from_instance(variables,
                                                                       var_decl_order,
                                                                       "/tmp",
                                                                       batsim_command)

    # Let's update those values
    variables['working_directory'] = working_directory = wd
    variables['output_directory'] = output_directory = od

    logger.info("Variables = {}".format(variables))

    # Let's create the directories if needed
    create_dir_if_not_exists(working_directory)
    create_dir_if_not_exists(output_directory)

    # Let's set the working directory
    os.chdir(working_directory)

    logger.info('Working directory: {wd}'.format(wd=os.getcwd()))
    logger.info('Output directory: {od}'.format(od=output_directory))

    # Let's create a variable definition file in the instance output directory
    variables_filename = '{out}/variables.bash'.format(out=output_directory)
    put_variables_in_file(variables, var_decl_order, variables_filename)

    if not args.post_only:
        # Let the execution be started
        # Commands before instance execution
        if len(commands_before_execution) > 0:
            pre_commands_dir = '{instance_out_dir}/pre_commands'.format(
                instance_out_dir=output_directory)
            pre_commands_output_dir = '{commands_dir}/out'.format(
                commands_dir=pre_commands_dir)
            create_dir_if_not_exists(pre_commands_dir)
            create_dir_if_not_exists(pre_commands_output_dir)

            nb_chars_command_ids = int(
                1 + math.log10(len(commands_before_execution)))

            for command_id in range(len(commands_before_execution)):
                command_name = 'command' + \
                    str(command_id).zfill(nb_chars_command_ids)
                output_command_filename = '{commands_dir}/{name}.bash'.format(
                    commands_dir=pre_commands_dir,
                    name=command_name)
                output_subscript_filename = '{commands_dir}/{name}_sub'.format(
                    commands_dir=pre_commands_dir,
                    name=command_name)

                if not execute_command(command=commands_before_execution[command_id],
                                       working_directory=working_directory,
                                       variables_filename=variables_filename,
                                       output_script_filename=output_command_filename,
                                       output_subscript_filename=output_subscript_filename,
                                       output_script_output_dir=pre_commands_output_dir,
                                       command_name=command_name):
                    sys.exit(1)

        # Instance execution
        if not execute_one_instance(working_directory=working_directory,
                                    output_directory=output_directory,
                                    batsim_command=batsim_command,
                                    interpreted_batsim_command=interpreted_batsim_command,
                                    sched_command=sched_command,
                                    variables_filename=variables_filename,
                                    timeout=timeout,
                                    do_not_execute=do_not_execute):
            sys.exit(2)

    # Commands after instance execution
    if len(commands_after_execution) > 0:
        post_commands_dir = '{instance_out_dir}/post_commands'.format(
            instance_out_dir=output_directory)
        post_commands_output_dir = '{commands_dir}/out'.format(
            commands_dir=post_commands_dir)
        create_dir_if_not_exists(post_commands_dir)
        create_dir_if_not_exists(post_commands_output_dir)

        nb_chars_command_ids = int(
            1 + math.log10(len(commands_after_execution)))

        for command_id in range(len(commands_after_execution)):
            command_name = 'command' + \
                str(command_id).zfill(nb_chars_command_ids)
            output_command_filename = '{commands_dir}/{name}.bash'.format(
                commands_dir=post_commands_dir,
                name=command_name)
            output_subscript_filename = '{commands_dir}/{name}_sub'.format(
                commands_dir=post_commands_dir,
                name=command_name)

            if not execute_command(command=commands_after_execution[command_id],
                                   working_directory=working_directory,
                                   variables_filename=variables_filename,
                                   output_script_filename=output_command_filename,
                                   output_subscript_filename=output_subscript_filename,
                                   output_script_output_dir=post_commands_output_dir,
                                   command_name=command_name):
                sys.exit(3)

    # Everything went succesfully, let's return 0
    sys.exit(0)


if __name__ == '__main__':
    main()

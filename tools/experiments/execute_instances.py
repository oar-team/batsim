#!/usr/bin/python2

import argparse
import yaml
import os
import sys
import hashlib
from execo import *
from execo_engine import *
from execute_one_instance import *

def retrieve_dirs_from_instances(variables, working_directory):
    filename_ok = False
    while not filename_ok:
        r = random_string()
        script_filename = '{wd}/{rand}_script.sh'.format(wd=working_directory,
                                                         rand=r)
        output_dir_filename = '{wd}/{rand}_out_dir'.format(wd=working_directory,
                                                           rand=r)
        working_dir_filename = '{wd}/{rand}_working_dir'.format(wd=working_directory,
                                                                rand=r)
        filename_ok = not os.path.exists(script_filename) and not os.path.exists(output_dir_filename) and not os.path.exists(working_dir_filename)

    put_variables_in_file(variables, script_filename)

    # Let's add some directives to prepare the instance!
    text_to_add = "# Preparation\n"
    text_to_add += 'echo {v} > {f}\n'.format(v = "${base_output_directory}",
                                             f = output_dir_filename)
    text_to_add += 'echo {v} > {f}\n'.format(v = "${base_working_directory}",
                                             f = working_dir_filename)

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
    base_working_dir = f.read().strip()
    f.close()

    # Let's get the output directory
    f = open(output_dir_filename, 'r')
    base_output_dir = f.read().strip()
    f.close()

    # Let's remove temporary files
    delete_file_if_exists(script_filename)
    delete_file_if_exists(working_dir_filename)
    delete_file_if_exists(output_dir_filename)

    return (base_working_dir, base_output_dir)

def get_script_path():
    return os.path.dirname(os.path.realpath(sys.argv[0]))

class WorkersSharedData:
    def __init__(self,
                 sweeper,
                 implicit_instances,
                 explicit_instances,
                 nb_workers_finished,
                 base_working_directory,
                 base_output_directory,
                 base_variables):
        self.sweeper = sweeper
        self.implicit_instances = implicit_instances
        self.explicit_instances = explicit_instances
        self.nb_workers_finished = nb_workers_finished
        self.base_working_directory = base_working_directory
        self.base_output_directory = base_output_directory
        self.base_variables = base_variables

class WorkerLifeCycleHandler(ProcessLifecycleHandler):
    def __init__(self,
                 hostname,
                 local_rank,
                 data):
        self.hostname = hostname
        self.host = Host(address = hostname)
        self.local_rank = local_rank
        self.data = data
        self.comb = None
        self.comb_basename = None

    def execute_next_instance(self):
        assert(self.comb == None)
        if len(self.data.sweeper.get_remaining()) > 0:
            # Let's get the next instance to compute
            self.comb = self.data.sweeper.get_next()
            logger.info('Worker ({hostname}, {local_rank}) got comb {comb}'.format(
                    hostname = self.hostname,
                    local_rank = self.local_rank,
                    comb = self.comb))

            if self.comb != None:
                (desc_filename, combname, command) = prepare_instance(
                    comb = self.comb,
                    explicit_instances = self.data.explicit_instances,
                    implicit_instances = self.data.implicit_instances,
                    base_working_directory = self.data.base_working_directory,
                    base_output_directory = self.data.base_output_directory,
                    base_variables = self.data.base_variables)
                self.combname = combname
                self._launch_process(instance_command = command)
            else:
                logger.info('Worker ({hostname}, {local_rank}) finished'.format(
                    hostname = self.hostname,
                    local_rank = self.local_rank))
                self.data.nb_workers_finished += 1
        else:
            # If there is no more work to do, this worker stops
            logger.info('Worker ({hostname}, {local_rank}) finished'.format(
                    hostname = self.hostname,
                    local_rank = self.local_rank))
            self.data.nb_workers_finished += 1

    def _launch_process(self,
                        instance_command):
        assert(self.comb != None)
        # Logging
        logger.info('Worker ({hostname}, {local_rank}) launches command {cmd}'.format(
                    hostname = self.hostname,
                    local_rank = self.local_rank,
                    cmd = instance_command))

        # Launching the process
        if self.hostname == 'localhost':
            process = Process(cmd = instance_command,
                              kill_subprocesses = True,
                              cwd = self.data.base_working_directory,
                              lifecycle_handlers = [self])
            process.start()
        else:
            process = SshProcess(cmd = instance_command,
                                 host = self.host,
                                 kill_subprocesses = True,
                                 cwd = self.data.base_working_directory,
                                 lifecycle_handlers = [self])
            process.start()

    def end(self, process):
        assert(self.comb != None)

        # Let's log the process's output
        create_dir_if_not_exists('{base_output_dir}/instances/output/'.format(
                                    base_output_dir = self.data.base_output_directory))

        write_string_into_file(process.stdout,
                               '{base_output_dir}/instances/output/{combname}.stdout'.format(
                                base_output_dir = self.data.base_output_directory,
                                combname = self.combname))
        write_string_into_file(process.stderr,
                               '{base_output_dir}/instances/output/{combname}.stderr'.format(
                                base_output_dir = self.data.base_output_directory,
                                combname = self.combname))

        # Let's mark whether the computation was successful
        if process.finished_ok:
            self.data.sweeper.done(self.comb)
            # Logging
            logger.info('Worker ({hostname}, {local_rank}) finished comb {comb} '
                        'successfully'.format(
                            hostname = self.hostname,
                            local_rank = self.local_rank,
                            comb = self.comb))
        else:
            logger.warning('Worker ({hostname}, {local_rank}) finished comb '
                           '{comb} unsuccessfully'.format(
                            hostname = self.hostname,
                            local_rank = self.local_rank,
                            comb = self.comb))
            self.data.sweeper.skip(self.comb)

        # Let's clear current instance variables
        self.comb = None
        self.combname = None

        self.execute_next_instance()

def prepare_instance(comb,
                     explicit_instances,
                     implicit_instances,
                     base_working_directory,
                     base_output_directory,
                     base_variables):
    if comb['explicit']:
        return prepare_explicit_instance(explicit_instances = explicit_instances,
                                         instance_id = comb['instance_id'],
                                         base_output_directory = base_output_directory,
                                         base_variables = base_variables)
    else:
        return prepare_implicit_instance(implicit_instances = implicit_instances,
                                         comb = comb,
                                         base_output_directory = base_output_directory,
                                         base_variables = base_variables)

def prepare_implicit_instance(implicit_instances,
                              comb,
                              base_output_directory,
                              base_variables):
    # Let's retrieve instance
    instance = implicit_instances[comb['instance_name']]
    generic_instance = instance['generic_instance']
    sweep = instance['sweep']

    # Let's copy it
    instance = generic_instance.copy()

    # Let's handle the variables
    if not 'variables' in instance:
        instance['variables'] = {}

    # Let's add sweep variables into the variable map
    for var_name in sweep:
        assert(var_name in comb)
        var_val = comb[var_name]
        if isinstance(var_val, tuple):
            instance['variables'][var_name] = list(var_val)
        else:
            instance['variables'][var_name] = var_val

    # Let's add the base_variables into the instance's variables
    instance['variables'].update(base_variables)

    # Let's generate a combname
    combname = 'implicit'
    combname_suffix = ''

    for var in sweep:
        val = comb[var]
        val_to_use = val
        if isinstance(val, tuple):
            val_to_use = val[0]
        combname_suffix += '__{var}={val}'.format(var = var,
                                                  val = val_to_use)

    combname += combname_suffix

    # Let's write a YAML description file corresponding to the instance
    desc_dir = '{out}/instances'.format(out = base_output_directory)
    desc_filename = '{dir}/{combname}.yaml'.format(dir = desc_dir,
                                                   combname = combname)

    create_dir_if_not_exists('{out}/instances'.format(out = base_output_directory))
    yaml_content = yaml.dump(instance, default_flow_style = False)

    write_string_into_file(yaml_content, desc_filename)

    # Let's prepare the launch command
    instance_command = '{exec_script} {desc_filename}'.format(
                            exec_script = execute_one_instance_script,
                            desc_filename = desc_filename)
    return (desc_filename, combname, instance_command)

def prepare_explicit_instance(explicit_instances,
                              instance_id,
                              base_output_directory,
                              base_variables):
    # Let's retrieve the instance
    instance = explicit_instances[instance_id]

    # Let's use base_variables into some fields of the instance
    for var_name in ['working_directory', 'output_directory', 'name']:
        if var_name in instance:
            val = evaluate_variables_in_string(instance[var_name],
                                               base_variables)
            instance[var_name] = val

    # Let's handle the variables
    if not 'variables' in instance:
        instance['variables'] = {}

    # Let's use base_variables into the variables of the instance
    for var_name in instance['variables']:
        var_val = instance['variables'][var_name]
        new_val = evaluate_variables_in_string(var_val, base_variables)
        instance['variables'][var_name] = new_val

    # Let's add the base_variables into the instance's variables
    instance['variables'].update(base_variables)

    # Let's write a YAML description file corresponding to the instance
    desc_dir = '{out}/instances'.format(out = base_output_directory)
    combname = 'explicit_{id}'.format(id = instance_id)
    desc_filename = '{dir}/{combname}.yaml'.format(dir = desc_dir,
                                                   combname = combname)

    create_dir_if_not_exists('{out}/instances'.format(out = base_output_directory))
    write_string_into_file(yaml.dump(instance, default_flow_style=False),
                           desc_filename)

    # Let's prepare the launch command
    instance_command = '{exec_script} {desc_filename}'.format(
                            exec_script = execute_one_instance_script,
                            desc_filename = desc_filename)
    return (desc_filename, combname, instance_command)

def retrieve_hostlist_from_mpi_hostfile(hostfile):
    hosts = set()
    f = open(hostfile, 'r')
    for line in f:
        host = line.split(' ')[0]
        hosts.insert(host)
    return list(hosts)

def generate_instances_combs(explicit_instances,
                             implicit_instances):
    # Let's generate explicit instances first.
    # Theses instances are directly written in the description file
    explicit_instances_comb = []
    if explicit_instances:
        nb_explicit = len(explicit_instances)
        explicit_instances_comb = [HashableDict(
                                   {'explicit':True,
                                   'instance_name':explicit_instances[x]['name'],
                                   'instance_id':x})
                                    for x in range(nb_explicit)]

    # Let's handle implicit instances now.
    # Theses instances define a parameter space sweeping and all combinations
    # should be executed.
    implicit_instances_comb = []
    if implicit_instances:
        for implicit_instance_name in implicit_instances:
            implicit_instance = implicit_instances[implicit_instance_name]
            if 'sweep' in implicit_instance:
                sweep_var = implicit_instance['sweep'].copy()
                # Let's transform lists into tuples (we need hashable objects to call sweep later)
                #print("before", sweep_var)
                for sweep_var_key in sweep_var:
                    sweep_var_value = sweep_var[sweep_var_key]
                    #print("sweep_var_value=",sweep_var_value)
                    for list_i in range(len(sweep_var_value)):
                        if isinstance(sweep_var_value[list_i], list):
                            #print('LIST !!!')
                            sweep_var_value[list_i] = tuple(sweep_var_value[list_i])
                    #print('\n')
                #print("after", sweep_var)
                #print('\n')
                sweep_var['explicit'] = [False]
                sweep_var['instance_name'] = [implicit_instance_name]
                implicit_instances_comb = implicit_instances_comb + sweep(sweep_var)

    return explicit_instances_comb + implicit_instances_comb

def execute_instances(base_working_directory,
                      base_output_directory,
                      base_variables,
                      host_list,
                      implicit_instances,
                      explicit_instances,
                      nb_workers_per_host):
    # Let's generate all instances that should be executed
    combs = generate_instances_combs(implicit_instances = implicit_instances,
                                     explicit_instances = explicit_instances)
    sweeper = ParamSweeper('{out}/sweeper'.format(out = base_output_directory),
                           combs)

    # Debug : let's mark all inprogress values as todo
    for comb in sweeper.get_inprogress():
        sweeper.cancel(comb)

    # Let's create data shared by all workers
    worker_shared_data = WorkersSharedData(sweeper = sweeper,
                                           implicit_instances = implicit_instances,
                                           explicit_instances = explicit_instances,
                                           nb_workers_finished = 0,
                                           base_working_directory = base_working_directory,
                                           base_output_directory = base_output_directory,
                                           base_variables = base_variables)

    # Let's create all workers
    nb_workers = min(len(combs), len(host_list) * nb_workers_per_host)
    workers = []
    assert(nb_workers > 0)

    for local_rank in range(nb_workers_per_host):
        if len(workers) >= nb_workers:
            break
        for hostname in host_list:
            if len(workers) >= nb_workers:
                break
            worker = WorkerLifeCycleHandler(hostname = hostname,
                                            local_rank = local_rank,
                                            data = worker_shared_data)
            workers.append(worker)

    assert(len(workers) == nb_workers)

    # Let's start an instance on each worker
    for worker in workers:
        worker.execute_next_instance()

    # Let's wait for the completion of all workers
    while worker_shared_data.nb_workers_finished < nb_workers:
        sleep(1)

    # Let's check that all instances have been executed successfully
    success = len(sweeper.get_skipped()) == 0
    logger.info('{} instances have been executed successfully'.format(len(sweeper.get_done())))
    if not success:
        logger.warning('{} instances have been skipped'.format(len(sweeper.get_skipped())))
    return success

def main():
    script_description = '''
Lauches several Batsim instances.
An instance can be represented by a tuple (platform, workload, algorithm).
Each workload is described in a YAML file (look at execute_one_instance.py for
more details.

These instances can also be grouped into one YAML file. Examples of such files
can be found in the instances_examples subdirectory.
'''

    # Program parameters parsing
    p = argparse.ArgumentParser(description = script_description)

    p.add_argument('instances_description_filename',
                   type = str,
                   help = 'The name of the YAML file which describes the instance. '
                          'Beware, this argument is not subjected to the working '
                          'directory parameter.')

    p.add_argument('--mpi_hostfile',
                   type = str,
                   help = 'If given, the set of available hosts is retrieved '
                          'the given MPI hostfile')

    p.add_argument('--nb_workers_per_host',
                   type = int,
                   default = 1,
                   help = 'Sets the number of workers that should be allocated '
                          'to each host.')

    p.add_argument('-bwd', '--base_working_directory',
                   type = str,
                   default = None,
                   help = 'If set, the instance will be executed in the given '
                          'directory. This value has higher priority than the '
                          'one that might be given in the description file. '
                          'If unset, the script working directory is used instead')

    p.add_argument('-bod', '--base_output_directory',
                   type = str,
                   default = None,
                   help = 'If set, the outputs of the current script will be '
                          'put into the given directory. This value has higher '
                          'priority than the one that might be given in the '
                          'description file. If a value is set, it might be '
                          'either absolute or relative to the working directory. '
                          ' If unset, the working directory is used instead')

    g = p.add_mutually_exclusive_group()

    g.add_argument('--pre_only',
                   action = 'store_true',
                   help = "If set, only the commands which precede instances' "
                          'executions will be executed.')

    g.add_argument('--post_only',
                   action = 'store_true',
                   help = "If set, only the commands which go after instances' "
                          'executions will be executed.')

    args = p.parse_args()

    # Some basic checks
    assert(args.nb_workers_per_host >= 1)

    # Let's read the YAML file content to get the real parameters
    desc_file = open(args.instances_description_filename, 'r')
    desc_data = yaml.load(desc_file)

    base_working_directory = os.getcwd()
    base_output_directory = os.getcwd()
    commands_before_instances = []
    commands_after_instances = []
    base_variables = {}
    host_list = ['localhost']

    if args.mpi_hostfile:
        host_list = retrieve_hostlist_from_mpi_hostfile(args.mpi_hostfile)

    if 'base_working_directory' in desc_data:
        base_working_directory = str(desc_data['base_working_directory'])
    if 'base_output_directory' in desc_data:
        base_output_directory = str(desc_data['base_output_directory'])
    if 'commands_before_instances' in desc_data:
        commands_before_instances = [str(command) for command in desc_data['commands_before_instances']]
    if 'commands_after_instances' in desc_data:
        commands_after_instances = [str(command) for command in desc_data['commands_after_instances']]
    if 'base_variables' in desc_data:
        base_variables = dict(desc_data['base_variables'])

    if args.base_working_directory:
        base_working_directory = args.base_working_directory

    if args.base_output_directory:
        base_output_directory = args.base_output_directory

    # Let's add some base_variables
    base_variables['base_working_directory'] = base_working_directory
    base_variables['base_output_directory'] = base_output_directory

    # Let's retrieve bwd and owd (they might need some bash interpretation)
    (base_working_directory, base_output_directory) = retrieve_dirs_from_instances(base_variables, "/tmp")

    # Let's update those variables
    base_variables['base_working_directory'] = base_working_directory
    base_variables['base_output_directory'] = base_output_directory

    # Let's check that variables are fine
    check_variables(base_variables)

    # Let's update the working directory
    os.chdir(base_working_directory)

    logger.info('Base working directory: {wd}'.format(wd = os.getcwd()))
    logger.info('Base output directory: {od}'.format(od = base_output_directory))

    # Let's get instances from the description file
    implicit_instances = {}
    explicit_instances = []

    if 'implicit_instances' in desc_data:
        implicit_instances = desc_data['implicit_instances']

    if 'explicit_instances' in desc_data:
        explicit_instances = desc_data['explicit_instances']

    if not args.post_only:
        # Let the execution be started
        # Commands before instance execution
        # for command in commands_before_instances:
        #     if not execute_command(working_directory = base_working_directory,
        #                            command = command,
        #                            variables = variables):
        #         sys.exit(1)

        if args.pre_only:
            sys.exit(0)

        # Instances' execution
        if not execute_instances(base_working_directory = base_working_directory,
                                 base_output_directory = base_output_directory,
                                 base_variables = base_variables,
                                 host_list = host_list,
                                 implicit_instances = implicit_instances,
                                 explicit_instances = explicit_instances,
                                 nb_workers_per_host = args.nb_workers_per_host):
            sys.exit(2)

    # Commands after instance execution
    # for command in commands_after_instances:
    #     if not execute_command(working_directory = base_working_directory,
    #                            command = command,
    #                            variables = variables):
    #         sys.exit(3)

    # Everything went succesfully, let's return 0
    sys.exit(0)

if __name__ == '__main__':
    execute_one_instance_script = '{script_dir}/execute_one_instance.py'.format(
                                    script_dir = get_script_path())
    assert(os.path.isfile(execute_one_instance_script))
    main()

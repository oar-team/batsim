#!/usr/bin/python2

import argparse
import yaml
import os
import sys
from execo import *
from execo_engine import *
import execute_one_instance

class WorkerLifeCycleHandler(ProcessLifecycleHandler):
    def __init__(self, hosts, instances):
        self.hosts = hosts
        self.instances
        self.available_hosts = hosts
        self.process_map = {}


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
                sweep_var = implicit_instance['sweep']
                sweep_var['explicit'] = [False]
                sweep_var['instance_name'] = [implicit_instance_name]
                implicit_instances_comb = implicit_instances_comb + sweep(sweep_var)

    return explicit_instances_comb + implicit_instances_comb

def execute_instances(base_working_directory,
                      base_output_directory,
                      base_variables,
                      host_list,
                      implicit_instances,
                      explicit_instances):
    # Let's generate all instances that should be executed
    combs = generate_instances_combs(implicit_instances = implicit_instances,
                                     explicit_instances = explicit_instances)
    print(combs)
    sweeper = ParamSweeper('{out}/sweeper'.format(out = base_output_directory),
                           combs)

    return True

    '''
    TODO : iterate over instances with nthread = nhosts
    TODO : create temp files to run execute_one_instance.py
    TODO : launch (ssh)processes to run execute_one_instance.py'''

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

    os.chdir(base_working_directory)

    logger.info('Base working directory: {wd}'.format(wd = os.getcwd()))
    logger.info('Base output directory: {od}'.format(od = base_output_directory))

    # Let's add some base_variables
    base_variables['base_working_directory'] = base_working_directory
    base_variables['base_output_directory'] = base_output_directory

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
        for command in commands_before_instances:
            if not execute_command(working_directory = base_working_directory,
                                   command = command,
                                   variables = variables):
                sys.exit(1)

        if args.pre_only:
            sys.exit(0)

        # Instances' execution
        if not execute_instances(base_working_directory = base_working_directory,
                                 base_output_directory = base_output_directory,
                                 base_variables = base_variables,
                                 host_list = host_list,
                                 implicit_instances = implicit_instances,
                                 explicit_instances = explicit_instances):
            sys.exit(2)

    # Commands after instance execution
    for command in commands_after_instances:
        if not execute_command(working_directory = base_working_directory,
                               command = command,
                               variables = variables):
            sys.exit(3)

    # Everything went succesfully, let's return 0
    sys.exit(0)

if __name__ == '__main__':
    main()

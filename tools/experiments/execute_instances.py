#!/usr/bin/env python3

"""Execute several Batsim instances."""

import asyncio.subprocess
import argparse
import collections
import hashlib
import math
import os
import signal
import subprocess
import sys
import yaml

import pandas as pd

from execo_engine import logger, ParamSweeper, sweep, HashableDict

from execute_one_instance import check_variables, delete_file_if_exists
from execute_one_instance import create_dir_if_not_exists, execute_command
from execute_one_instance import is_valid_identifier, write_string_into_file
from execute_one_instance import execute_command_inner
from execute_one_instance import random_string, put_variables_in_file


class hashabledict(dict):
    """A homemade hashable dictionary."""

    def __hash__(self):
        """Constructor."""
        return hash(tuple(sorted(self.items())))


class hashablelist(list):
    """A homemade hashable list."""

    def __hash__(self):
        """Constructor."""
        return hash(tuple(self))


def flatten_dict(init, lkey=''):
    """Flatten the values of a dictionary."""
    ret = {}
    for rkey, val in init.items():
        key = lkey + rkey
        if isinstance(val, dict):
            ret.update(flatten_dict(val, key + '__'))
        else:
            ret[key] = val
    return ret

# This function uses ugly globals (search for the 'global' keyword to find
# them)


def signal_handler(signal, frame):
    """Callback function called on interruption."""
    print('Interruption signal received!')

    if ('instances_df' in globals()) and (len(instances_df) > 0):
        print('Saving current status in {}'.format(instances_df_filename))

        done_df = pd.DataFrame([{
            'instance_id': instance_id_from_comb(x, hash_length),
            'status': 'done'} for x in sweeper.get_done()])
        skipped_df = pd.DataFrame([{
            'instance_id': instance_id_from_comb(x, hash_length),
            'status': 'skipped'} for x in sweeper.get_skipped()])
        remaining_df = pd.DataFrame([{
            'instance_id': instance_id_from_comb(x, hash_length),
            'status': 'remaining'} for x in sweeper.get_remaining()])
        inprogress_df = pd.DataFrame([{
            'instance_id': instance_id_from_comb(x, hash_length),
            'status': 'inprogress'} for x in sweeper.get_inprogress()])

        status_df = pd.concat(
            [done_df, skipped_df, remaining_df, inprogress_df])

        joined_df = pd.merge(instances_df, status_df, on='instance_id')
        joined_df.to_csv(instances_df_filename, index=False, na_rep='NA')

    print('Aborting.')
    sys.exit(1)


def instance_id_from_comb(comb, hash_length):
    """Return the instance id from a combination."""
    fdict = flatten_dict(comb)
    fdict = collections.OrderedDict(sorted(fdict.items()))
    return hashlib.sha1(str(fdict).encode()).hexdigest()[:hash_length]


def retrieve_dirs_from_instances(variables,
                                 variables_declaration_order,
                                 working_directory):
    """Retrieve the directories from instances."""
    filename_ok = False
    while not filename_ok:
        r = random_string()
        script_filename = '{wd}/{rand}_script.sh'.format(
            wd=working_directory, rand=r)
        output_dir_filename = '{wd}/{rand}_out_dir'.format(
            wd=working_directory, rand=r)
        working_dir_filename = '{wd}/{rand}_working_dir'.format(
            wd=working_directory, rand=r)
        filename_ok = (not os.path.exists(script_filename) and
                       not os.path.exists(output_dir_filename) and
                       not os.path.exists(working_dir_filename))

    put_variables_in_file(
        variables, variables_declaration_order, script_filename)

    # Let's add some directives to prepare the instance!
    text_to_add = "# Preparation\n"
    text_to_add += 'echo {v} > {f}\n'.format(v="${base_output_directory}",
                                             f=output_dir_filename)
    text_to_add += 'echo {v} > {f}\n'.format(v="${base_working_directory}",
                                             f=working_dir_filename)

    # Let's append the directives in the file
    f = open(script_filename, 'a')
    f.write(text_to_add)
    f.close()

    # Let's execute the script
    p = subprocess.run('bash {f}'.format(f=script_filename), shell=True,
                       stdout=subprocess.PIPE)
    if p.returncode != 0:
        raise Exception('Script returned %d (expected 0)'.format(p.returncode))

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

# todo: check that explicit instances have a 'name' field typed as a string


def check_sweep(sweeps):
    """Check whether the sweep parameters are valid."""
    # Let's check that sweeping values are consistent (same type, same fields)
    if not isinstance(sweeps, dict):
        logger.error("Invalid sweep: must be a dict")
        sys.exit(1)

    list_sizes = {}
    dict_keys = {}
    var_types = {}
    used_identifiers = {}
    dicts_without_names = set()

    for var_name in sweeps:
        var_value = sweeps[var_name]
        if not isinstance(var_value, list):
            logger.error("Invalid sweep variable {v}: associated value is not "
                         "a list".format(v=var_name))
            sys.exit(1)
        if len(var_value) < 1:
            logger.error("Invalid sweep variable {v}: length of list value "
                         "must be > 0".format(v=var_name))
            sys.exit(1)
        for element in var_value:
            # Let's check that all values have the same type
            t = type(element)
            if var_name in var_types:
                if t != var_types[var_name]:
                    logger.error(
                        "Invalid sweep variable {v}: all possible values are "
                        "not of the same type".format(v=var_name))
                    sys.exit(1)
            else:
                var_types[var_name] = t

            # Let's do more check on lists and dicts
            if t == list:
                length = len(element)
                if var_name in list_sizes:
                    if length != list_sizes[var_name]:
                        logger.error("Invalid sweep variable {v}: all possible"
                                     " values must be of the same type (and "
                                     "since they are lists, they should have "
                                     "the same length)".format(v=var_name))
                        sys.exit(1)
                else:
                    list_sizes[var_name] = length
                if length < 1:
                    logger.error("Invalid sweep variable {v}: lists must be "
                                 "non-empty".format(v=var_name))
                    sys.exit(1)
                used_name = element[0]
                if not is_valid_identifier(used_name):
                    logger.error("Invalid sweep variable {v}: first element "
                                 "({f}) must be a valid identifier because it "
                                 "is used to create files".format(v=var_name,
                                                                  f=used_name))
                    sys.exit(1)
                # Let's check that all names are unique
                if var_name in used_identifiers:
                    if used_name in used_identifiers[var_name]:
                        logger.error("Invalid sweep variable {v}: first "
                                     "element value ({f}) must be "
                                     "unique".format(v=var_name,
                                                     f=used_name))
                        sys.exit(1)
                    else:
                        used_identifiers[var_name].add(used_name)
                else:
                    used_identifiers[var_name] = set([used_name])

            elif t == dict:
                keys = element.keys()
                if var_name in dict_keys:
                    if keys != dict_keys[var_name]:
                        logger.error("Invalid sweep variable {v}: all possible"
                                     " values must be of the same type (and"
                                     " since they are dicts, they should have"
                                     " the same keys".format(v=var_name))
                        sys.exit(1)
                else:
                    dict_keys[var_name] = keys
                if len(keys) < 1:
                    logger.error("Invalid sweep variable {v}: dicts must be "
                                 "non-empty".format(v=var_name))
                    sys.exit(1)
                if 'name' in element:
                    used_name = element['name']
                else:
                    used_name = element.values()[0]
                    dicts_without_names.add(var_name)
                if not is_valid_identifier(used_name):
                    logger.error("Invalid sweep variable {v}: the name got "
                                 "from dict {d} (name={n}, got either from "
                                 "the 'name' field if it exists or the first "
                                 "value otherwise) is not a valid identifier. "
                                 "It must be because it is used to create "
                                 "files.".format(v=var_name, d=element,
                                                 n=used_name))
                    sys.exit(1)
                # Let's check that all names are unique
                if var_name in used_identifiers:
                    if used_name in used_identifiers[var_name]:
                        logger.error("Invalid sweep variable {v}: the name "
                                     "got from dict {d} (name={n}, got either "
                                     "from the 'name' field if it exists or "
                                     "the first value otherwise) must be "
                                     "unique".format(v=var_name, d=element,
                                                     n=used_name))
                        sys.exit(1)
                    else:
                        used_identifiers[var_name].add(used_name)
                else:
                    used_identifiers[var_name] = set([used_name])

    if len(dicts_without_names) > 0:
        logger.warning("Different dictionary variables do not have a "
                       "'name' key: {d}".format(d=dicts_without_names))


def get_script_path():
    """Return where the script is located."""
    return os.path.dirname(os.path.realpath(sys.argv[0]))


async def instance_runner(data, hostname, local_rank):
    """Runner in charge of executing instances asynchronously, one by one."""
    while len(data.sweeper.get_remaining()) > 0:
        comb = data.sweeper.get_next()
        if comb is None:
            break
        compute_comb = True
        if data.instances_subset_to_recompute is not None:
            compute_comb = comb in data.instances_subset_to_recompute

        if compute_comb:
            (desc_filename, instance_id, combname, command) = prepare_instance(
                comb=comb,
                explicit_instances=data.explicit_instances,
                implicit_instances=data.implicit_instances,
                base_working_directory=data.base_working_directory,
                base_output_directory=data.base_output_directory,
                base_variables=data.base_variables,
                post_commands_only=comb in data.instances_post_cmds_only,
                generate_only=data.generate_only,
                hash_length=data.hash_length)

            logger.info('Worker ({host},{rank}) got {iid} ({comb})'.format(
                host=hostname, rank=local_rank,
                iid=instance_id, comb=comb))

            # Remote launch via taktuk
            rmt = """ssh {host} '{cmd}'""".format(host=hostname,
                                                  cmd=command)

            # Preparing the launch
            if hostname != 'localhost':
                command = rmt

            create_dir_if_not_exists('{bod}/instances/output/'.format(
                bod=data.base_output_directory))
            stdout_filename = '{out}/instances/output/{iid}.stdout'.format(
                out=data.base_output_directory,
                iid=instance_id)
            stderr_filename = '{out}/instances/output/{iid}.stderr'.format(
                out=data.base_output_directory,
                iid=instance_id)

            stdout_file = open(stdout_filename, 'wb')
            stderr_file = open(stderr_filename, 'wb')

            # Launch the instance
            logger.info('Worker ({host},{rank}) runs {iid}'.format(
                host=hostname, rank=local_rank, iid=instance_id))
            p, _ = await execute_command_inner(command, stdout_file,
                                               stderr_file)
            if p.returncode == 0:
                logger.info('Worker ({host},{rank}) finished {iid}'.format(
                    host=hostname, rank=local_rank, iid=instance_id))
                data.sweeper.done(comb)
            else:
                show_instance_details = True

                if (p.returncode == 3):
                    logger.error('Worker ({host},{rank}) finished {iid} '
                                 '(returncode={code}). Instance is marked as '
                                 'done because it failed in the post-commands '
                                 'section.'.format(
                                     host=hostname, rank=local_rank,
                                     iid=instance_id, code=p.returncode))
                    data.post_command_failed = True
                    data.sweeper.done(comb)
                elif (p.returncode == 4) and data.generate_only:
                    logger.info('Worker ({host},{rank}) finished {iid} '
                                '(returncode={code} expected, as not executing'
                                ' the instance has been asked.)'.format(
                                    host=hostname, rank=local_rank,
                                    iid=instance_id, code=p.returncode))
                    data.sweeper.skip(comb)
                    show_instance_details = False
                else:
                    logger.error('Worker ({host},{rank}) finished {iid} '
                                 '(returncode={code}).'.format(
                                     host=hostname, rank=local_rank,
                                     iid=instance_id, code=p.returncode))
                    data.sweeper.skip(comb)

                if show_instance_details:
                    logger.info('\n\n----- begin of instance {iid} log -----'
                                .format(iid=instance_id))
                    display_process_output_on_error('Instance ' + instance_id,
                                                    stdout_file, stderr_file)
                    logger.info('----- end of instance {iid} log -----\n\n'
                                .format(iid=instance_id))
        else:
            logger.info('Worker ({host},{rank}) skipped {iid}'.format(
                host=hostname, rank=local_rank, iid=instance_id))
            data.sweeper.skip(comb)

    logger.info('Worker ({host},{rank}) finished'.format(
        host=hostname, rank=local_rank))


class WorkersSharedData:
    """Stores Data shared by the different workers."""

    def __init__(self,
                 sweeper,
                 instances_post_cmds_only,
                 implicit_instances,
                 explicit_instances,
                 nb_workers_finished,
                 base_working_directory,
                 base_output_directory,
                 base_variables,
                 generate_only,
                 hash_length,
                 instances_subset_to_recompute):
        """Constructor."""
        self.sweeper = sweeper
        self.instances_post_cmds_only = instances_post_cmds_only
        self.implicit_instances = implicit_instances
        self.explicit_instances = explicit_instances
        self.nb_workers_finished = nb_workers_finished
        self.base_working_directory = base_working_directory
        self.base_output_directory = base_output_directory
        self.base_variables = base_variables
        self.generate_only = generate_only
        self.hash_length = hash_length
        self.post_command_failed = False
        self.instances_subset_to_recompute = instances_subset_to_recompute


def prepare_instance(comb,
                     explicit_instances,
                     implicit_instances,
                     base_working_directory,
                     base_output_directory,
                     base_variables,
                     post_commands_only=False,
                     generate_only=False,
                     hash_length=8):
    """Prepare an instance (generate scripts...)."""
    if comb['explicit']:
        return prepare_explicit_instance(
            explicit_instances=explicit_instances,
            comb=comb,
            instance_id=comb['instance_id'],
            base_output_directory=base_output_directory,
            base_variables=base_variables,
            post_commands_only=post_commands_only,
            generate_only=generate_only,
            hash_length=hash_length)
    else:
        return prepare_implicit_instance(
            implicit_instances=implicit_instances,
            comb=comb,
            base_output_directory=base_output_directory,
            base_variables=base_variables,
            post_commands_only=post_commands_only,
            generate_only=generate_only,
            hash_length=hash_length)


def prepare_implicit_instance(implicit_instances,
                              comb,
                              base_output_directory,
                              base_variables,
                              post_commands_only=False,
                              generate_only=False,
                              hash_length=8):
    """Prepare an implicit instance."""
    # Let's retrieve instance
    instance = implicit_instances[comb['instance_name']]
    generic_instance = instance['generic_instance']
    sweep = instance['sweep']

    # Let's copy it
    instance = generic_instance.copy()

    # Let's handle the variables
    if 'variables' not in instance:
        instance['variables'] = {}

    # Let's add sweep variables into the variable map
    for var_name in sweep:
        if var_name not in comb:
            raise Exception('{} is not in the comb map!'.format(var_name))

        var_val = comb[var_name]
        if isinstance(var_val, hashablelist):
            instance['variables'][var_name] = list(var_val)
        elif isinstance(var_val, hashabledict):
            instance['variables'][var_name] = dict(var_val)
        else:
            instance['variables'][var_name] = var_val

    # Let's add the base_variables into the instance variables
    instance['variables'].update(base_variables)

    # Let's add the instance_id into the instance variables
    instance_id = instance_id_from_comb(comb, hash_length)
    dict_iid = {'instance_id': instance_id}
    instance['variables'].update(dict_iid)

    # Let's add the instance_number into the instance variables
    instance_number = instance_id_to_instance_number[instance_id]
    dict_inum = {'instance_number': instance_number}
    instance['variables'].update(dict_inum)

    # Let's generate a combname
    combname = 'implicit'
    combname_suffix = ''

    for var in sweep:
        val = comb[var]
        val_to_use = val
        if isinstance(val, hashablelist):
            val_to_use = val[0]
        elif isinstance(val, hashabledict):
            if 'name' in val:
                val_to_use = val['name']
            else:
                val_to_use = val.itervalues().next()
        combname_suffix += '__{var}_{val}'.format(var=var,
                                                  val=val_to_use)

    combname += combname_suffix

    # Let's write a YAML description file corresponding to the instance
    desc_dir = '{out}/instances'.format(out=base_output_directory)
    desc_filename = '{dir}/{instance_id}.yaml'.format(dir=desc_dir,
                                                      instance_id=instance_id)

    create_dir_if_not_exists(
        '{out}/instances'.format(out=base_output_directory))
    yaml_content = yaml.dump(instance, default_flow_style=False)

    write_string_into_file(yaml_content, desc_filename)

    # Let's prepare the command options
    options = ""
    if post_commands_only:
        options = ' --post_only'
    elif generate_only:
        options = ' --do_not_execute'

    # Let's prepare the launch command
    instance_command = '{exec_script}{opt} {desc_filename}'.format(
        exec_script=execute_one_instance_script,
        opt=options,
        desc_filename=desc_filename)
    return (desc_filename, instance_id, combname, instance_command)


def prepare_explicit_instance(explicit_instances,
                              comb,
                              instance_id,
                              base_output_directory,
                              base_variables,
                              post_commands_only=False,
                              generate_only=False,
                              hash_length=8):
    """Prepare an explicit instance."""
    # Let's retrieve the instance
    instance = explicit_instances[instance_id]

    # Let's handle the variables
    if 'variables' not in instance:
        instance['variables'] = {}

    # Let's add the base_variables into the instance variables
    instance['variables'].update(base_variables)

    # Let's add the instance_id into the instance variables
    instance_id = instance_id_from_comb(comb, hash_length)
    dict_iid = {'instance_id': instance_id}
    instance['variables'].update(dict_iid)

    # Let's add the instance_number into the instance variables
    instance_number = instance_id_to_instance_number[instance_id]
    dict_inum = {'instance_number': instance_number}
    instance['variables'].update(dict_inum)

    # Let's write a YAML description file corresponding to the instance
    desc_dir = '{out}/instances'.format(out=base_output_directory)
    combname = 'explicit_{id}'.format(id=instance_id)
    desc_filename = '{dir}/{instance_id}.yaml'.format(dir=desc_dir,
                                                      instance_id=instance_id)

    create_dir_if_not_exists(
        '{out}/instances'.format(out=base_output_directory))
    write_string_into_file(yaml.dump(instance, default_flow_style=False),
                           desc_filename)

    # Let's prepare the command options
    options = ""
    if post_commands_only:
        options = ' --post_only'
    elif generate_only:
        options = ' --do_not_execute'

    # Let's prepare the launch command
    instance_command = '{exec_script}{opt} {desc_filename}'.format(
        exec_script=execute_one_instance_script,
        opt=options,
        desc_filename=desc_filename)
    return (desc_filename, instance_id, combname, instance_command)


def retrieve_hostlist_from_mpi_hostfile(hostfile):
    """Return a hostlist from a MPI hostfile."""
    hosts = set()
    f = open(hostfile, 'r')
    for line in f:
        line = line.strip()
        host = line.split(' ')[0]
        hosts.add(host)
    return list(hosts)


def generate_instances_combs(explicit_instances,
                             implicit_instances):
    """Generate combinations of instances."""
    # Let's generate explicit instances first.
    # Theses instances are directly written in the description file
    explicit_instances_comb = []
    if explicit_instances:
        nb_explicit = len(explicit_instances)
        explicit_instances_comb = [HashableDict({
            'explicit': True,
            'instance_name': explicit_instances[x]['name'],
            'instance_id':x}) for x in range(nb_explicit)]

    # Let's handle implicit instances now.
    # Theses instances define a parameter space sweeping and all combinations
    # should be executed.
    implicit_instances_comb = []
    if implicit_instances:
        for implicit_instance_name in implicit_instances:
            implicit_instance = implicit_instances[implicit_instance_name]
            if 'sweep' in implicit_instance:
                sweep_var = implicit_instance['sweep'].copy()
                check_sweep(sweep_var)
                # Let's make sure all objects are hashable so sweep() can be
                # called
                for sweep_var_key in sweep_var:
                    sweep_var_value = sweep_var[sweep_var_key]
                    if not isinstance(sweep_var_value, list):
                        raise Exception('{} is not a list!'.format(
                            sweep_var_value))
                    for list_i in range(len(sweep_var_value)):
                        if isinstance(sweep_var_value[list_i], list):
                            sweep_var_value[list_i] = hashablelist(
                                sweep_var_value[list_i])
                        elif isinstance(sweep_var_value[list_i], dict):
                            sweep_var_value[list_i] = hashabledict(
                                sweep_var_value[list_i])

                sweep_var['explicit'] = [False]
                sweep_var['instance_name'] = [implicit_instance_name]
                implicit_instances_comb = implicit_instances_comb + \
                    sweep(sweep_var)

    return explicit_instances_comb + implicit_instances_comb


def execute_instances(base_working_directory,
                      base_output_directory,
                      base_variables,
                      host_list,
                      implicit_instances,
                      explicit_instances,
                      nb_workers_per_host,
                      recompute_all_instances,
                      recompute_instances_post_commands,
                      generate_only=False,
                      mark_as_cancelled_lambda=None,
                      only_compute_marked_cancelled=False):
    """Execute the instances."""
    # Let's generate all instances that should be executed
    combs = generate_instances_combs(implicit_instances=implicit_instances,
                                     explicit_instances=explicit_instances)
    global sweeper
    sweeper = ParamSweeper('{out}/sweeper'.format(out=base_output_directory),
                           combs)

    # Preparation of the instances data frame
    all_combs = sweeper.get_remaining() | sweeper.get_done() | \
        sweeper.get_skipped() | sweeper.get_inprogress()

    global instance_id_to_instance_number
    instance_id_to_instance_number = dict()

    global hash_length
    hash_length = 8
    rows_list = []
    instance_number = 0
    for comb in all_combs:
        row_dict = flatten_dict(comb)
        row_dict['instance_id'] = instance_id_from_comb(comb, hash_length)
        row_dict['instance_number'] = instance_number
        instance_id_to_instance_number[
            row_dict['instance_id']] = instance_number

        rows_list.append(row_dict)
        instance_number = instance_number + 1

    # Let's generate the instances data frame
    global instances_df
    global instances_df_filename
    instances_df_filename = '{bod}/instances/instances_info.csv'.format(
        bod=base_output_directory)

    instances_df = pd.DataFrame(rows_list)

    # Let's check that instance ids are unique
    if len(instances_df) != len(instances_df['instance_id'].unique()):
        raise Exception('Instance IDs are not unique ({}!={})! The hash length'
                        ' might be too small.'
                        .format(len(instances_df),
                                len(instances_df['instance_id'].unique())))

    # Let's save the dataframe into a csv file
    create_dir_if_not_exists('{base_output_dir}/instances'.format(
        base_output_dir=base_output_directory))
    instances_df.to_csv(instances_df_filename, index=False, na_rep='NA')

    instances_subset_to_recompute = None

    if mark_as_cancelled_lambda == '':
        if len(sweeper.get_done()) > 0:
            logger.info('The first finished combination is shown below.')
            logger.info(next(iter(sweeper.get_done())))
            sys.exit(0)
        elif len(sweeper.get_inprogress()) > 0:
            logger.info('The first inprogress combination is shown below.')
            logger.info(next(iter(sweeper.get_inprogress())))
            sys.exit(0)
        elif len(sweeper.get_remaining()) > 0:
            logger.info('The first todo combination is shown below.')
            logger.info(next(iter(sweeper.get_remaining())))
            sys.exit(0)
        else:
            logger.info('It seems that there is no combination... Try again.')
            sys.exit(0)
    elif mark_as_cancelled_lambda is not None:
        logger.info('Trying to find instances to mark as cancelled. '
                    'Lambda parameter is "{p}". Lambda filter string is "{f}".'
                    .format(p='x', f=mark_as_cancelled_lambda))
        combs_to_mark = [y for y in filter(lambda x:
                                           eval(mark_as_cancelled_lambda),
                                           sweeper.get_done())]

        logger.info('{nb} instances will be marked as cancelled. Are you sure?'
                    ' Type "yes" to confirm.'.format(nb=len(combs_to_mark)))

        input_line = sys.stdin.readline().strip().lower()

        if input_line == 'yes':
            for comb_to_mark in combs_to_mark:
                sweeper.cancel(comb_to_mark)
            logger.info('{nb} instances have been marked as cancelled.'.format(
                nb=len(combs_to_mark)))
        else:
            logger.info('Aborted. No instance has been marked as cancelled')

        if only_compute_marked_cancelled:
            instances_subset_to_recompute = combs_to_mark

    # Let's mark all inprogress values as todo
    for comb in sweeper.get_inprogress():
        sweeper.cancel(comb)

    instances_post_cmds_only = set()
    if recompute_instances_post_commands:
        for comb in sweeper.get_done():
            instances_post_cmds_only.add(comb)
            sweeper.cancel(comb)

    if recompute_all_instances:
        instances_post_cmds_only.clear()
        for comb in sweeper.get_done():
            sweeper.cancel(comb)

    # Let's create data shared by all workers
    worker_shared_data = WorkersSharedData(
        sweeper=sweeper,
        instances_post_cmds_only=instances_post_cmds_only,
        implicit_instances=implicit_instances,
        explicit_instances=explicit_instances,
        nb_workers_finished=0,
        base_working_directory=base_working_directory,
        base_output_directory=base_output_directory,
        base_variables=base_variables,
        generate_only=generate_only,
        hash_length=hash_length,
        instances_subset_to_recompute=instances_subset_to_recompute)

    # Let's create all workers
    nb_workers = min(len(combs), len(host_list) * nb_workers_per_host)
    workers = []
    if nb_workers <= 0:
        raise Exception('There are 0 workers!')

    for local_rank in range(nb_workers_per_host):
        if len(workers) >= nb_workers:
            break
        for hostname in host_list:
            if len(workers) >= nb_workers:
                break
            worker = (hostname, local_rank)
            workers.append(worker)

    if len(workers) != nb_workers:
        raise Exception('Bad number of workers!')

    # Let's run all the workers, and wait for them to finish
    loop = asyncio.get_event_loop()
    loop.run_until_complete(asyncio.gather(
        *[instance_runner(worker_shared_data, host, rank)
          for (host, rank) in workers]))

    # Let's compute which instances failed
    done_df = pd.DataFrame([{
        'instance_id': instance_id_from_comb(x, hash_length),
        'status': 'done'} for x in sweeper.get_done()])
    skipped_df = pd.DataFrame([{
        'instance_id': instance_id_from_comb(x, hash_length),
        'status': 'skipped'} for x in sweeper.get_skipped()])
    status_df = pd.concat([done_df, skipped_df])

    # Let's log this into the CSV file
    joined_df = pd.merge(instances_df, status_df, on='instance_id')
    joined_df.to_csv(instances_df_filename, index=False, na_rep='NA')

    # Let's check that all instances have been executed successfully
    success = len(sweeper.get_skipped()) == 0
    logger.info('Number of successfully executed instances: {}'.format(
        len(sweeper.get_done())))
    if not success:
        logger.warning('Number of skipped instances: {}'.format(
            len(sweeper.get_skipped())))
        logger.warning('Information about these instances can be found in '
                       'file {}'.format('{bod}/instances/instances_info.csv'
                                        .format(bod=base_output_directory)))
    return (success, worker_shared_data.post_command_failed)


def main():
    """
    Script entry point.

    Essentially:
    1. Parse input arguments
    2. (Execute pre-commands)
    3. Execute all instances
    4. (Execute post-commands)
    """
    script_description = '''
Lauches several Batsim instances.
An instance can be represented by a tuple (platform, workload, algorithm).
Each workload is described in a YAML file (look at execute_one_instance.py for
more details.

These instances can also be grouped into one YAML file. Examples of such files
can be found in the instances_examples subdirectory.
'''

    signal.signal(signal.SIGINT, signal_handler)

    # Program parameters parsing
    p = argparse.ArgumentParser(description=script_description)

    p.add_argument(
        'instances_description_filename',
        type=str,
        help='The name of the YAML file which describes the instance. '
             'Beware, this argument is not subjected to the working '
             'directory parameter.')

    p.add_argument(
        '--mpi_hostfile',
        type=str,
        help='If given, the set of available hosts is retrieved '
             'the given MPI hostfile')

    p.add_argument(
        '--nb_workers_per_host',
        type=int,
        default=1,
        help='Sets the number of workers that should be allocated '
             'to each host.')

    p.add_argument(
        '-bwd', '--base_working_directory',
        type=str,
        default=None,
        help='If set, the instance will be executed in the given '
             'directory. This value has higher priority than the '
             'one that might be given in the description file. '
             'If unset, the script working directory is used instead')

    p.add_argument(
        '-bod', '--base_output_directory',
        type=str,
        default=None,
        help='If set, the outputs of the current script will be '
             'put into the given directory. This value has higher '
             'priority than the one that might be given in the '
             'description file. If a value is set, it might be '
             'either absolute or relative to the working directory. '
             ' If unset, the working directory is used instead')

    gm = p.add_mutually_exclusive_group()
    gm.add_argument(
        '-m', '--mark_instances_as_cancelled',
        type=str,
        default=None,
        help='Allows to mark some instances as cancelled, which '
             'will force them to be recomputed. The parameter '
             'is a lambda expression used to filter which '
             'instances should be recomputed. If the parameter '
             'is empty, a combination example is displayed. '
             'The lambda parameter is named "x".')

    gm.add_argument(
        '-M', '--recompute_some_instances_only',
        type=str,
        default=None,
        help='Quite the same as -m, but this parameter limits the '
             'parameter space to the one described in this '
             'parameter.')

    p.add_argument(
        '--skip_global_pre',
        action='store_true',
        help='If set, skips the commands which should be executed '
             'before running any instance.')

    p.add_argument(
        '--skip_global_post',
        action='store_true',
        help='If set, skips the commands which should be executed '
             'after running all the instances.')

    g = p.add_mutually_exclusive_group()

    g.add_argument(
        '--pre_only',
        action='store_true',
        help="If set, only the commands which precede instances "
             'executions will be executed.')

    g.add_argument(
        '--post_only',
        action='store_true',
        help="If set, only the commands which go after instances' "
             'executions will be executed.')

    g.add_argument(
        '-g', '--generate_only',
        action='store_true',
        help='If set, the instances will not be executed but only '
             'generated. Commands before execution will be run, '
             'commands before each instance too.')

    g.add_argument(
        '-r', '--recompute_all_instances',
        action='store_true',
        help="If set, all instances will be recomputed. "
             "By default, Execo's cache allows to avoid "
             "recomputations of already done instances")

    g.add_argument(
        '-p', '--recompute_instances_post_commands',
        action='store_true',
        help='If set, the post_commands of the instances which '
             'are already done will be computed before trying to '
             'execute the other instances')

    g.add_argument(
        '-pg', '--recompute_already_done_post_commands',
        action='store_true',
        help='Does quite the same as '
             '--recompute_instances_post_commands, but does not '
             'try to execute skipped instances')

    args = p.parse_args()

    # Some basic checks
    if args.nb_workers_per_host < 1:
        raise Exception('There should be at least one worker per host!')

    # Let's read the YAML file content to get the real parameters
    desc_file = open(args.instances_description_filename, 'r')
    desc_data = yaml.load(desc_file)

    base_working_directory = os.getcwd()
    base_output_directory = os.getcwd()
    commands_before_instances = []
    commands_after_instances = []
    base_variables = {}

    generate_only = False
    if args.generate_only:
        generate_only = True

    recompute_all_instances = False
    if args.recompute_all_instances:
        recompute_all_instances = True

    recompute_instances_post_commands = False
    if args.recompute_instances_post_commands:
        recompute_instances_post_commands = True

    # This option is just a combination of other options :D
    recompute_already_done_post_commands = False
    if args.recompute_already_done_post_commands:
        recompute_already_done_post_commands = True
        recompute_instances_post_commands = True
        generate_only = True

    mark_as_cancelled_lambda = None
    only_compute_marked_cancelled = False
    if args.mark_instances_as_cancelled is not None:
        mark_as_cancelled_lambda = args.mark_instances_as_cancelled
    elif args.recompute_some_instances_only is not None:
        mark_as_cancelled_lambda = args.recompute_some_instances_only
        only_compute_marked_cancelled = True

    host_list = ['localhost']
    if args.mpi_hostfile:
        host_list = retrieve_hostlist_from_mpi_hostfile(args.mpi_hostfile)

    if 'base_working_directory' in desc_data:
        base_working_directory = str(desc_data['base_working_directory'])
    if 'base_output_directory' in desc_data:
        base_output_directory = str(desc_data['base_output_directory'])
    if 'commands_before_instances' in desc_data:
        commands_before_instances = [str(command) for command in desc_data[
            'commands_before_instances']]
    if 'commands_after_instances' in desc_data:
        commands_after_instances = [str(command) for command in desc_data[
            'commands_after_instances']]
    if 'base_variables' in desc_data:
        base_variables = dict(desc_data['base_variables'])

    if args.base_working_directory:
        base_working_directory = args.base_working_directory

    if args.base_output_directory:
        base_output_directory = args.base_output_directory

    # Let's add some base_variables
    base_variables['base_working_directory'] = base_working_directory
    base_variables['base_output_directory'] = base_output_directory

    # Let's check that variables are fine
    (var_ok, var_decl_order) = check_variables(base_variables)
    if not var_ok:
        sys.exit(1)

    # Let's retrieve bwd and owd (they might need some bash interpretation)
    (base_working_directory, base_output_directory) = (
        retrieve_dirs_from_instances(base_variables, var_decl_order, "/tmp"))

    # Let's update those variables
    base_variables['base_working_directory'] = base_working_directory
    base_variables['base_output_directory'] = base_output_directory

    # Let's create the directories if needed
    create_dir_if_not_exists(base_working_directory)
    create_dir_if_not_exists(base_output_directory)

    # Let's update the working directory
    os.chdir(base_working_directory)

    logger.info('Base working directory: {wd}'.format(wd=os.getcwd()))
    logger.info('Base output directory: {od}'.format(od=base_output_directory))

    # Let's create a variable definition file in the instance output directory
    base_variables_filename = '{out}/base_variables.bash'.format(
        out=base_output_directory)
    put_variables_in_file(base_variables, var_decl_order,
                          base_variables_filename)

    # Let's get instances from the description file
    implicit_instances = {}
    explicit_instances = []

    if 'implicit_instances' in desc_data:
        implicit_instances = desc_data['implicit_instances']

    if 'explicit_instances' in desc_data:
        explicit_instances = desc_data['explicit_instances']

    post_command_failed = False

    if not args.post_only:
        # Commands before instances execution
        if len(commands_before_instances) > 0 and not args.skip_global_pre:
            pre_commands_dir = '{bod}/pre_commands'.format(
                bod=base_output_directory)
            pre_commands_output_dir = '{bod}/out'.format(
                bod=pre_commands_dir)
            create_dir_if_not_exists(pre_commands_dir)
            create_dir_if_not_exists(pre_commands_output_dir)

            nb_chars_command_ids = int(
                1 + math.log10(len(commands_before_instances)))

            for command_id in range(len(commands_before_instances)):
                command_name = 'command' + \
                    str(command_id).zfill(nb_chars_command_ids)
                output_command_filename = '{commands_dir}/{name}.bash'.format(
                    commands_dir=pre_commands_dir,
                    name=command_name)
                output_subscript_filename = '{commands_dir}/{name}_sub'.format(
                    commands_dir=pre_commands_dir,
                    name=command_name)

                if not execute_command(
                   command=commands_before_instances[command_id],
                   working_directory=base_working_directory,
                   variables_filename=base_variables_filename,
                   output_script_filename=output_command_filename,
                   output_subscript_filename=output_subscript_filename,
                   output_script_output_dir=pre_commands_output_dir,
                   command_name=command_name):
                    sys.exit(1)

        if args.pre_only:
            sys.exit(0)

        # Instances' execution
        (should_continue, post_command_failed) = execute_instances(
            base_working_directory=base_working_directory,
            base_output_directory=base_output_directory,
            base_variables=base_variables,
            host_list=host_list,
            implicit_instances=implicit_instances,
            explicit_instances=explicit_instances,
            nb_workers_per_host=args.nb_workers_per_host,
            recompute_all_instances=recompute_all_instances,
            recompute_instances_post_commands=(
                recompute_instances_post_commands),
            generate_only=generate_only,
            mark_as_cancelled_lambda=mark_as_cancelled_lambda,
            only_compute_marked_cancelled=only_compute_marked_cancelled)
        if not should_continue and not recompute_already_done_post_commands:
            sys.exit(2)

    # Commands after instances execution
    if len(commands_after_instances) > 0 and \
       (not generate_only or recompute_already_done_post_commands) and \
       not args.skip_global_pre:
        post_commands_dir = '{bod}/post_commands'.format(
            bod=base_output_directory)
        post_commands_output_dir = '{bod}/out'.format(
            bod=post_commands_dir)
        create_dir_if_not_exists(post_commands_dir)
        create_dir_if_not_exists(post_commands_output_dir)

        nb_chars_command_ids = int(
            1 + math.log10(len(commands_after_instances)))

        for command_id in range(len(commands_after_instances)):
            command_name = 'command' + \
                str(command_id).zfill(nb_chars_command_ids)
            output_command_filename = '{commands_dir}/{name}.bash'.format(
                commands_dir=post_commands_dir,
                name=command_name)
            output_subscript_filename = '{commands_dir}/{name}_sub'.format(
                commands_dir=post_commands_dir,
                name=command_name)

            if not execute_command(
               command=commands_after_instances[command_id],
               working_directory=base_working_directory,
               variables_filename=base_variables_filename,
               output_script_filename=output_command_filename,
               output_subscript_filename=output_subscript_filename,
               output_script_output_dir=post_commands_output_dir,
               command_name=command_name):
                sys.exit(1)

    # Everything went succesfully, let's return 0
    if post_command_failed:
        print('Everything went quite successfully, '
              'but some post-commands failed...')
        sys.exit(7)
    sys.exit(0)


if __name__ == '__main__':
    execute_one_instance_script = '{sdir}/execute_one_instance.py'.format(
        sdir=get_script_path())
    if not os.path.isfile(execute_one_instance_script):
        raise Exception('{} is not a file'.format(execute_one_instance_script))
    main()

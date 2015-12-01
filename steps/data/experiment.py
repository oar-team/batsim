#!/usr/bin/python3

import subprocess
import os
import csv

import multiprocessing

# Options that might be changed
generate_json_files = True
launch_experiments = True
experiment_id = 18
experiment_name = 'good_seeds'
leave_on_already_existing_experiment = False

glob_batsim_directory = '/root/batsim/'
glob_batsim_executable = 'build/batsim'

glob_perl_sched_directory = '/root/batch-simulator/'
glob_perl_sched_executable = 'scripts/run_schedule_simulator.pl'
perl_sched_variants = {'BASIC': 0, 'BEST_EFFORT_CONTIGUOUS': 1,
                       'CONTIGUOUS': 2, 'BEST_EFFORT_LOCAL': 3, 'LOCAL': 4}

platform = '/root/griffon_modified.xml'
master_host_name = 'master_host0'

input_swf_trace = 'CEA-Curie-2011-2.1-cln.swf'
generator_executable = '/root/swfToJsonConverter.py'

# Let's create the result directory if needed, or leave if we may erase
# something useful
result_directory = os.getcwd() + '/exp' + str(experiment_id) + \
    experiment_name + '/'
json_directory = result_directory + 'json/'
print('result_directory = ', result_directory)

if os.path.isdir(result_directory):
    subfiles = [name for name in os.listdir(result_directory)
                if os.path.isfile(os.path.join(result_directory, name))]
    if len(subfiles) > 0 and leave_on_already_existing_experiment:
        print('The result directory already exists and contains files.'
              'Aborting to avoid deleting previously obtained results')
        exit(1)
else:
    print('Creating result directory ' + result_directory)
    os.makedirs(result_directory)
    os.makedirs(json_directory)


def zpad(val, n):
    bits = str(val).split('.')
    if len(bits) == 2:
        return "%s.%s" % (bits[0].zfill(n), bits[1])
    else:
        return "%s" % bits[0].zfill(n)

# Let's create a result file which summarize the results

experiment_output_csv_file = open(result_directory + 'results.csv', 'w')
writer = csv.DictWriter(
    experiment_output_csv_file,
    fieldnames=['variant',
                'comp_factor',
                'comm_factor',
                'platform_file',
                'json_file',
                'cmax',
                'locality_factor',
                'nb_jobs',
                'success_rate',
                'nb_jobs_killed',
                'contiguous_jobs_number',
                'local_jobs_number',
                'runtime',
                'jobs_random_seed',
                'jobs_execution_time_boundary_ratio'])
writer.writeheader()

# This function execute both programs in different processes, parse their
# output to write a line of the CSV output file


def run_batsim_and_perl_scheduler(batsim_platform,
                                  json_file,
                                  perl_sched_variant,
                                  comp_factor,
                                  comm_factor,
                                  batsim_master_host_name="master_host",
                                  batsim_directory=glob_batsim_directory,
                                  batsim_executable=glob_batsim_executable,
                                  perl_sched_directory=glob_perl_sched_directory,
                                  perl_sched_executable=glob_perl_sched_executable,
                                  verbose=False,
                                  perl_sched_delay=15,
                                  jobs_random_seed=0,):

    instance_name = str(perl_sched_variant) + '_' + zpad(comp_factor, 10) + \
        '_' + zpad(comm_factor, 10) + '_' + zpad(jobs_random_seed, 3)
    cluster_size = 16

    socket = '/tmp/' + instance_name

    perl_sched_command = '{} {} {} {} {} {}'.format(perl_sched_executable, cluster_size, perl_sched_variants[
                                                    perl_sched_variant], perl_sched_delay, socket, json_file)
    perl_sched_args = str.split(perl_sched_command, sep=' ')

    batsim_log_level = 'critical'
    batsim_export_prefix = result_directory + instance_name
    batsim_command = "{} --socket={} --master-host={} --export={} -- {} {} --log=batsim.thresh:{} --log=network.thresh:{} --log=utils.thresh:{}".format(batsim_executable,
                                                                                                                                                        socket, batsim_master_host_name, batsim_export_prefix, batsim_platform, json_file, batsim_log_level, batsim_log_level, batsim_log_level)
    batsim_args = str.split(batsim_command, sep=' ')

    # Let's create the processes
    perl_sched_process = subprocess.Popen(
        perl_sched_args, cwd=perl_sched_directory, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    batsim_process = subprocess.Popen(
        batsim_args, cwd=batsim_directory, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Let's wait for them to finish
    batsim_errcode = batsim_process.wait()
    perl_out, perl_err = perl_sched_process.communicate()

    # If batsim did not finish successfully, let's display it
    if batsim_errcode != 0:
        batsim_out, batsim_err = batsim_process.communicate()

        print('Batsim failed on instance', instance_name)
        print('batsim_out :', str(batsim_out))
        print('batsim_err :', str(batsim_err))
        return

    # Let's retrieve some information about the execution from the scheduler
    # output
    cmax, contiguous_jobs_number, local_jobs_number, locality_factor, runtime = str.split(
        str(perl_out)[2:-1], sep=' ')
    cmax = float(cmax)
    contiguous_jobs_number = float(contiguous_jobs_number)
    local_jobs_number = float(local_jobs_number)
    locality_factor = float(locality_factor)
    runtime = float(runtime)

    # Let's retrieve some other informations from the batsim output (csv file)
    batsim_csv_output_file = open(batsim_export_prefix + '_schedule.csv', 'r')
    reader = csv.DictReader(batsim_csv_output_file)
    batsim_results = [row for row in reader]
    batsim_result = batsim_results[0]

    # Let's write a csv line about this run
    return {'variant': perl_sched_variant, 'comp_factor': comp_factor, 'comm_factor': comm_factor, 'cmax': cmax, 'locality_factor': locality_factor,
            'contiguous_jobs_number': contiguous_jobs_number, 'local_jobs_number': local_jobs_number, 'runtime': runtime,
            'nb_jobs': batsim_result['nb_jobs'], 'success_rate': batsim_result['success_rate'],
            'nb_jobs_killed': batsim_result['nb_jobs_killed'], 'platform_file': batsim_platform, 'json_file': json_file, 'jobs_random_seed': jobs_random_seed,
            'jobs_execution_time_boundary_ratio': batsim_result['jobs_execution_time_boundary_ratio']}

# Generate a JSON file (via another script call)


def generateJsonFile(inputSWFFile, outputJsonFiles, compFactors, commFactors, nb_jobs, max_job_height, platform_size, jobMinWidth, jobMaxWidth, randomSeed=0):

    outputJsonFilesJ = ' '.join(outputJsonFiles)
    compFactorsJ = ' '.join([str(x) for x in compFactors])
    commFactorsJ = ' '.join([str(x) for x in commFactors])

    generator_command = (
        "{} -cpu '{}' -com '{}' -prj {} -crs {} "
        "--randomizeCommunications 1 -mjh {} -jwf 1000 -fst 0 "
        "-pf {} -q --jobMinWidth {} --jobMaxWidth {} "
        "-- {} '{}'").format(generator_executable,
                             compFactorsJ,
                             commFactorsJ,
                             nb_jobs,
                             randomSeed,
                             max_job_height,
                             platform_size,
                             jobMinWidth,
                             jobMaxWidth,
                             inputSWFFile,
                             outputJsonFilesJ)

    generator_command.replace('\xa0', ' ')

    generator_process = subprocess.Popen(
        generator_command, cwd=os.getcwd(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if generator_process.wait() != 0:
        out, err = generator_process.communicate()
        print('Failed to generate json files', outputJsonFiles)
        print(generator_command)
        print('out = ', out)
        print('err = ', err)
        exit(1)

# What should we run ?
compFactors = [1e6]
#commFactors = [x for x in range(0,1000000000) if x%1e6==0]
commFactors = [x * 1e7 for x in range(0, 40)]
#commFactors = [0]
jobs_to_use = [300]
maximum_job_height = [16]
variants_to_use = ['BEST_EFFORT_CONTIGUOUS',
                   'CONTIGUOUS', 'BEST_EFFORT_LOCAL', 'LOCAL']
#variants_to_use = ['BASIC']
seeds_to_use = [x for x in range(0, 20)]
#seeds_to_use = [0,1,2,4,17]


num_cores = multiprocessing.cpu_count()


# Let's generate all the json files
def generateOneInstanceGroup(t):
    compFactorsI, comFactorsI, num_jobs, max_job_height, seed = t
    json_filenames = []
    for compFactor in compFactorsI:
        for comFactor in comFactorsI:
            json_filenames.append('{}{}_{}_{}_{}_{}.json'.format(
                json_directory, num_jobs, max_job_height, zpad(compFactor, 10), zpad(comFactor, 10), zpad(seed, 3)))
    generateJsonFile(input_swf_trace, json_filenames, compFactorsI, comFactorsI, num_jobs,
                     max_job_height, 128, jobMinWidth=60 * 60, jobMaxWidth=60 * 60 * 2, randomSeed=seed)

if generate_json_files:
    for num_jobs in jobs_to_use:
        for max_job_height in maximum_job_height:
            for seed in seeds_to_use:
                generateOneInstanceGroup(
                    (compFactors, commFactors, num_jobs, max_job_height, seed))


def launchOneInstance(t):
    json_file, absolute_json_file, current_variant = t
    nb_jobs, max_job_height, compFactor, comFactor, seed = str.split(json_file[
                                                                     :-5], sep='_')
    nb_jobs = int(nb_jobs)
    max_job_height = int(max_job_height)
    comFactor = float(comFactor)
    compFactor = float(compFactor)
    seed = int(seed)

    print('Running instance: (nb_jobs={}, max_job_height={}, comp={}, com={}, var={}, seed={})'.format(
        nb_jobs, max_job_height, compFactor, comFactor, current_variant, seed))
    return run_batsim_and_perl_scheduler(batsim_platform=platform, json_file=absolute_json_file, perl_sched_variant=current_variant,
                                         comp_factor=compFactor, comm_factor=comFactor, batsim_master_host_name=master_host_name, jobs_random_seed=seed)


# Let's launch batsim and the scheduler on every json
if launch_experiments:
    json_files = [name for name in os.listdir(
        json_directory) if os.path.isfile(os.path.join(json_directory, name))]
    absolute_json_files = [json_directory + name for name in json_files]

    json_zipped_files = zip(json_files, absolute_json_files)
    instances_to_launch = []

    for (json_file, absolute_json_file) in json_zipped_files:
        for variant in variants_to_use:
            instances_to_launch.append(
                (json_file, absolute_json_file, variant))

    for t in instances_to_launch:
        d = launchOneInstance(t)
        writer.writerow(d)

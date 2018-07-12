Experiment tools
================

This directory contains scripts that can be used to make experimenting with Batsim easier.

These scripts are written in Python 2 but should be compatible with Python 3.

Dependencies
------------

### Execo
We use Execo to manage processes easily, to avoid to recompute instances which have already
been computed and to handle remote launches of processes.

We changed Execo a little bit to make it fit our needs. These modifications have not been
merged with the main repo yet, so our fork must be used to use these experiment scripts at
the moment.

#### Installation
``` bash
cd $${simctn_dir}
cd execo
git checkout 406f4fa06 # cwd support
# Make sure you have no execo already installed before the next step
# Otherwise, conflicts might occur (try pip2 uninstall execo)
pip2 install --user .
```


Execute one instance
--------------------

The file [execute_one_instance.py](./execute_one_instance.py) is a python 2 script in charge of
executing one Batsim instance.
Concretely, an instance is composed of:
  - One command which says how Batsim should be executed (on which platform, on which workload,
    where to put output files, which socket to use...)
  - One command which says how the scheduler should be executed (...)
  - Optional: Commands to execute before executing (Batsim, scheduler)
  - Optional: Commands to execute after executing (Batsim, scheduler)

This script handles process quite finely and using it might avoid you a lot of problems.
For examples, such scenarios are handled:
  - The scheduler is only executed once Batsim has effectively created the socket
  - If Batsim crashes, the scheduler is automatically killed a few seconds after the crash and vice versa.
  - Variables can easily be added and used. These variables are bash-complient (simple values, lists or dictionaries).

### Input definition
We chose to use YAML files as input of the [execute_one_instance.py](./execute_one_instance.py) script.

``` yaml
# If needed, the working directory can be specified within this file
# working_directory: ~/proj/batsim

# The different variables can be defined here
variables:
  platform: {"name":"small", "filename":"platforms/small_platform.xml"} # A dictionary
  workload: {"name":"tiny", "filename":"workload_profiles/test_workload_profile.json"} # Another dictionary
  pybatsim_algo: fillerSched # A simple variable
  useless_list: ["just_an_example", "mmh..."] # A list
# Some variables are automatically added into this dictionary:
#   - working_directory
#   - output_directory

# If needed, the output directory can be specified within this file
output_directory: /tmp/pyfiller/tiny

# The commands
batsim_command: batsim ${platform["filename"]} ${workload["filename"]} -e ${output_directory}/out -s ${output_directory}/socket
sched_command: python2 schedulers/pybatsim/launcher.py ${pybatsim_algo} ${workload["filename"]} -s ${output_directory}/socket

# These commands will be executed before the instance, in this order
# One-line commands will be executed in a bash environment.
# Multi-line commands will be placed in a script of their own, from which
# variables can be accessed via environment variables.
commands_before_execution:
  - echo $(pwd) > ${output_directory}/.pwd
  - cp ${output_directory}/.pwd ${output_directory}/crap_file
  - |
    #!/bin/python2
    import os
    fname = os.environ['output_directory'] + '/python_output'
    f = open(fname, 'w')
    f.write("Hello from python2!\n")
    f.close()

# These commands will be executed after the instance, in this order
commands_after_execution:
  - cat ${output_directory}/crap_file
  - |
    #!/bin/bash
    cat ${output_directory}/out_schedule.csv | cut -d',' -f6 | tail -n 1 > ${output_directory}/makespan

# An upper bound on the instance's execution time can be set if needed
timeout: 10
```

Variables can be used to determine everything else:
  - Batsim and scheduler commands
  - Working directory and output directory

Furthermore, the working directory and the output directory are themselves placed into the variables,
which lets you use them into your commands.

### Usage
  The script usage is quite straightforward. It can be obtained via
``` bash
./execute_one_instance.py --help
```
Which shows something like this:
```
usage: execute_one_instance.py [-h] [-wd WORKING_DIRECTORY]
                               [-od OUTPUT_DIRECTORY]
                               instance_description_filename

Lauches one Batsim instance. An instance can be represented by a tuple
(platform, workload, algorithm). It is described in a YAML file, which is the
parameter of this script. Examples of such input files can be found in the
subdirectory instance_examples.

positional arguments:
  instance_description_filename
                        The name of the YAML file which describes the
                        instance. Beware, this argument is not subjected to
                        the working directory parameter.

optional arguments:
  -h, --help            show this help message and exit
  -wd WORKING_DIRECTORY, --working_directory WORKING_DIRECTORY
                        If set, the instance will be executed in the given
                        directory. This value has higher priority than the one
                        that might be given in the description file. If unset,
                        the script working directory is used instead
  -od OUTPUT_DIRECTORY, --output_directory OUTPUT_DIRECTORY
                        If set, the outputs of the current script will be put
                        into the given directory. This value has higher
                        priority than the one that might be given in the
                        description file. If a value is set, it might be
                        either absolute or relative to the working directory.
                        If unset, the working directory is used instead
```

Commands are executed from the working directory.  Outputs are written into the
output directory. These directories can be given via command-line arguments
or put into the YAML input file. If both are set, only the command-line
argument will be used.

If the working directory is set neither in command-line arguments nor in the YAML
file, the script working directory is used: it it from where you launch the
script.

If the output directory is set neither in command-line arguments nor in the YAML
file, its value is set as the one of the working directory.

### Output files
The script creates different files in the output directory:
  - batsim.stdout and batsim.stderr, corresponding to Batsim execution's outputs
  - sched.stdout and sched.stderr, corresponding to the scheduler execution's output
  - variables.bash, a bash script which initializes the variables which should be used in your commands
  - batsim_command.sh, a bash script in charge of running Batsim. This script sources
    variables.bash then executes the command you specified.
  - sched_command.sh, a bash script in charge of running the scheduler. This script sources
    variables.bash then executes the command you specified.
  - (Batsim output files, depending on your Batsim command)
  - (Sched output files, depending on your scheduler command)


Execute different instances
---------------------------

The file [execute_instances.py](./execute_instances.py) is a python 2 script in charge of
executing several Batsim instances.
A set of instances can be defined by:
  - A list of "explicit" instances: Instances directly written by the user
  - Different "implicit" instances, generated from a combination of parameters

The main advantages of this script are:
  - It can generate all implicit combinations by itself
  - It will try to execute each instance, showing any error that may occur
  - The execution will be done by the
    [execute_one_instance.py](./execute_one_instance.py) script to maintain
    separation of concerns.
  - It will remember which instances have been computed successfully and won't
    compute them again.
  - It will create a YAML input file for each instance, making debugging easier
  - It allows parallelism since different workers can be used. These workers can
    be local or distributed.
  - It will log the outputs of the different instances

### Input
The input of this script is also in YAML. An example is written below:

``` yaml
# This script should be called from Batsim's root directory

# If needed, the working directory of this script can be specified within this file
# base_working_directory: ~/proj/batsim

# If needed, the output directory of this script can be specified within this file
base_output_directory: ${base_working_directory}/instances_out

# If needed, base_variables can be defined
# base_variables:
#   - target_conference: IPDPS'2016
# base_output_directory and base_working_directory are
# added in the base_variables.

# The instances defined by a combination of parameters
implicit_instances:
  # Several set of combinations can be defined. This one is called 'implicit'.
  implicit:
    # The different parameters to explore.
    # Beware: sweep must be a dictionary whose keys are valid identifiers and
    # whose values are lists.
    sweep:
      platform :
        # We only define one platform whose name is homo128 and with a filename.
        # Giving a 'name' field to your dictionaries is good practice, because
        # it is used to generate instance YAML filenames for each combination.
        # If no 'name' is found, the first value is taken instead, which could
        # be dangerous.
        - {"name":"homo128", "filename":"platforms/energy_platform_homogeneous_no_net_128.xml"}
      workload :
        # We define two different workloads there. Please not their names MUST
        # be different to avoid different instances pointing to the same YAML
        # filename.
        - {"name":"tiny", "filename":"workload_profiles/test_workload_profile.json"}
        - {"name":"medium", "filename":"workload_profiles/batsim_paper_workload_example.json"}
      pybatsim_algo:
        # We use only one scheduling algorithm
        - fillerSched
    # Defines how each instance of 'implicit' should be computed
    generic_instance:
      # All base_variables are added into the variables of the instance.
      # Hence, base_working_directory can be used there for example.
      working_directory: ${base_working_directory}
      output_directory: ${base_output_directory}/results/${pybatsim_algo}_${workload["name"]}_${platform["name"]}
      # If you want your instances to be executable in parallel, you should
      # specify the socket which should be used to communicate.
      batsim_command: batsim ${platform["filename"]} ${workload["filename"]} -e ${output_directory}/out -s ${output_directory}/socket
      sched_command: python2 schedulers/pybatsim/launcher.py ${pybatsim_algo} ${workload["filename"]} -s ${output_directory}/socket

# You can define instances explicitely here.
# Beware: the explicit_instances value must be a list.
explicit_instances:
  - name : easybf_tiny_small
    output_directory: ${base_output_directory}/results/explicit/easybf_tiny_small
    variables: # We use simple values here, not dictionaries (but they could have been used!)
      platform: platforms/small_platform.xml
      workload: workload_profiles/test_workload_profile.json
      pybatsim_algo: easyBackfill
      # All base_variables are also copied here

    batsim_command: batsim ${platform} ${workload} -e ${output_directory}/out -s ${output_directory}/socket
    sched_command: python2 schedulers/pybatsim/launcher.py ${pybatsim_algo} ${workload} -s ${output_directory}/socket

# These commands will be executed before running the instances, in this order
# commands_before_instances:
#   - pwd > ./lost.txt

# These commands will be executed after running all the instances, in this order
# commands_after_instances:
#   - ls
```

### Usage
  The script usage is quite straightforward. It can be obtained via
``` bash
./execute_instances.py --help
```
Which shows something like this:
```
usage: execute_instances.py [-h] [--mpi_hostfile MPI_HOSTFILE]
                            [--nb_workers_per_host NB_WORKERS_PER_HOST]
                            [-bwd BASE_WORKING_DIRECTORY]
                            [-bod BASE_OUTPUT_DIRECTORY]
                            [--pre_only | --post_only]
                            instances_description_filename

Lauches several Batsim instances. An instance can be represented by a tuple
(platform, workload, algorithm). Each workload is described in a YAML file
(look at execute_one_instance.py for more details. These instances can also be
grouped into one YAML file. Examples of such files can be found in the
instances_examples subdirectory.

positional arguments:
  instances_description_filename
                        The name of the YAML file which describes the
                        instance. Beware, this argument is not subjected to
                        the working directory parameter.

optional arguments:
  -h, --help            show this help message and exit
  --mpi_hostfile MPI_HOSTFILE
                        If given, the set of available hosts is retrieved the
                        given MPI hostfile
  --nb_workers_per_host NB_WORKERS_PER_HOST
                        Sets the number of workers that should be allocated to
                        each host.
  -bwd BASE_WORKING_DIRECTORY, --base_working_directory BASE_WORKING_DIRECTORY
                        If set, the instance will be executed in the given
                        directory. This value has higher priority than the one
                        that might be given in the description file. If unset,
                        the script working directory is used instead
  -bod BASE_OUTPUT_DIRECTORY, --base_output_directory BASE_OUTPUT_DIRECTORY
                        If set, the outputs of the current script will be put
                        into the given directory. This value has higher
                        priority than the one that might be given in the
                        description file. If a value is set, it might be
                        either absolute or relative to the working directory.
                        If unset, the working directory is used instead
  --pre_only            If set, only the commands which precede instances'
                        executions will be executed.
  --post_only           If set, only the commands which go after instances'
                        executions will be executed.
```

This script follows the same rules that were defined for
[execute_one_instance.py](./execute_one_instance.py). Working directories can
be specified by command-line arguments or directly into the input YAML file.
The same priorities are used than those of the
[execute_one_instance.py](./execute_one_instance.py) script.

### Output files
Executing this script will generate different directories and files in the
base_output_directory:
```
.
├── instances
│   ├── [explicit YAML files]
│   ├── [implicit YAML files]
│   └── output
│       ├── [explicit instances execution output (stdout+strerr)]
│       └── [implicit instances execution output (stdout+strerr)]
├── [results, depending on your script]
└── sweeper
    └── [Execo internal files to store which instances have already been executed]
```

You can organise your results the way you want. In the above example script
they are put into a 'results' directory for example, with one subdirectory per
instance.

#!/bin/bash -ex
#       .
#      / \
#     / | \
#    /  |  \    This script should be called from Batsim root directory !
#   /   |   \
#  /         \
# /     o     \
# ‾‾‾‾‾‾‾‾‾‾‾‾‾

# Clean test output directories
\rm -rf /tmp/batsim_tests

# Run a travis server if needed
server_launched_by_me=0
r=$(ps faux | grep redis-server | grep -v grep | wc -l)
if [ $r -eq 0 ]
then
    redis-server>/dev/null &
    server_launched_by_me=1
fi

# Run input examples
tools/experiments/execute_one_instance.py \
    -od test/out/instance_examples/pftiny \
    ./tools/experiments/instance_examples/pybatsim_filler_tiny.yaml
tools/experiments/execute_one_instance.py \
    -od test/out/instance_examples/pfmedium \
    ./tools/experiments/instance_examples/pybatsim_filler_medium.yaml
tools/experiments/execute_instances.py \
    -bod test/out/instance_examples/pfmixed \
    ./tools/experiments/instances_examples/pyfiller_tiny_medium_mixed.yaml

# Run different tests
tools/experiments/execute_instances.py test/test_walltime.yaml
tools/experiments/execute_instances.py test/test_no_energy.yaml
tools/experiments/execute_instances.py test/test_space_sharing.yaml
tools/experiments/execute_instances.py test/test_energy.yaml
tools/experiments/execute_instances.py test/test_energy.yaml -bod /tmp/batsim_tests/sshprocess1 --mpi_hostfile test/mpi_hostfile_localhost
tools/experiments/execute_instances.py test/test_energy.yaml -bod /tmp/batsim_tests/sshprocess2 --mpi_hostfile test/mpi_hostfile_localhost --nb_workers_per_host 2
tools/experiments/execute_instances.py test/test_energy.yaml -bod /tmp/batsim_tests/sshprocess4 --mpi_hostfile test/mpi_hostfile_localhost --nb_workers_per_host 4
#tools/experiments/execute_instances.py test/test_smpi.yaml -bod test/out/smpi
tools/experiments/execute_instances.py test/test_long_simulation_time.yaml
tools/experiments/execute_instances.py test/test_batexec.yaml

# Let's stop the redis server if it has been launched by this script
if [ $server_launched_by_me -eq 1 ]
then
    killall redis-server
fi

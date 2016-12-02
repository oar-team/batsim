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
\rm -rf test/out/energy
\rm -rf test/out/instance_examples
\rm -rf test/out/long_simulation_time
\rm -rf test/out/no_energy
\rm -rf test/out/space_sharing
\rm -rf test/out/smpi
\rm -rf test/out/unique
\rm -rf test/out/walltime

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
tools/experiments/execute_instances.py test/test_unique.yaml -bod test/out/unique
tools/experiments/execute_instances.py test/test_walltime.yaml -bod test/out/walltime
tools/experiments/execute_instances.py test/test_no_energy.yaml -bod test/out/no_energy
tools/experiments/execute_instances.py test/test_space_sharing.yaml -bod test/out/space_sharing
tools/experiments/execute_instances.py test/test_energy.yaml -bod test/out/energy
#tools/experiments/execute_instances.py test/test_smpi.yaml -bod test/out/smpi
tools/experiments/execute_instances.py test/test_long_simulation_time.yaml -bod test/out/long_simulation_time

# Let's stop the redis server if it has been launched by this script
if [ $server_launched_by_me -eq 1 ]
then
    killall redis-server
fi

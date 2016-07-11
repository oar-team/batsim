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
\rm -rf test/out/unique
\rm -rf test/out/no_energy
\rm -rf test/out/space_sharing
\rm -rf test/out/energy

# Run different tests
tools/experiments/execute_instances.py test/test_unique.yaml -bod test/out/unique
tools/experiments/execute_instances.py test/test_no_energy.yaml -bod test/out/no_energy
tools/experiments/execute_instances.py test/test_space_sharing.yaml -bod test/out/space_sharing
tools/experiments/execute_instances.py test/test_energy.yaml -bod test/out/energy

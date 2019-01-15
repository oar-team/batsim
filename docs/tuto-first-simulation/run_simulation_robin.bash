#!/usr/bin/env bash
set -x

./retrieve_simulation_inputs.bash
./prepare_output_directory.bash

./show_versions.bash > /tmp/expe-out/show_versions.out

# This is a bash hack to test if DO_NOT_CHECK_VERSION is unset.
if [ -z ${DO_NOT_CHECK_VERSION+x} ]; then
    diff {.,/tmp/expe-out}/show_versions.out

    if [ $? -ne 0 ]; then
        echo "Versions are not those expected. Aborting."
        exit 1
    fi
fi

./run_robin_generate.bash
if [ $? -ne 0 ]; then
    echo "Generating the robin file was not successful. Aborting."
    exit 1
fi

./run_robin.bash
if [ $? -ne 0 ]; then
    echo "Simulation did not finish properly. Aborting."
    exit 1
fi

exit 0

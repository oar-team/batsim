#!/usr/bin/env bash

batsim -p /tmp/batsim-v2.0.0/platforms/cluster512.xml \
       -w /tmp/batsim-v2.0.0/workload_profiles/batsim_paper_workload_example.json \
       -m 'master_host0' \
       -e "/tmp/expe-out/out"

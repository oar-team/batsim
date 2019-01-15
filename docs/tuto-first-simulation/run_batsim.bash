#!/usr/bin/env bash

batsim -p /tmp/batsim-v3.0.0/platforms/cluster512.xml \
       -w /tmp/batsim-v3.0.0/workloads/test_batsim_paper_workload_seed1.json \
       -e "/tmp/expe-out/out"

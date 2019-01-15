#!/usr/bin/env bash

robin generate ./expe.yaml \
      --output-dir=/tmp/expe-out \
      --batcmd="batsim -p /tmp/batsim-v3.0.0/platforms/cluster512.xml -w /tmp/batsim-v3.0.0/workloads/test_batsim_paper_workload_seed1.json -e /tmp/expe-out/out" \
      --schedcmd='batsched -v easy_bf'

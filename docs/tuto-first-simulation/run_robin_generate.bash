#!/usr/bin/env bash

robin generate ./expe.yaml \
      --output-dir=/tmp/expe-out \
      --batcmd="batsim -p /tmp/batsim-src-stable/platforms/cluster512.xml -w /tmp/batsim-src-stable/workloads/test_batsim_paper_workload_seed1.json -e /tmp/expe-out/out_" \
      --schedcmd='batsched -v easy_bf'

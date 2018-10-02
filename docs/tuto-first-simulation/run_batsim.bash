#!/usr/bin/env bash

batsim -p ${BATSIM_DIR}/platforms/cluster512.xml \
       -w ${BATSIM_DIR}/workload_profiles/batsim_paper_workload_example.json \
       -m 'master_host0' \
       -e "${EXPE_RESULT_DIR}/out"

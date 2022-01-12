#!/usr/bin/env bash
set -eu

# start from a clean directory structure
EXPE_DIR=$(realpath ./expe)
rm -rf ${EXPE_DIR}

# create instances' directories
mkdir -p ${EXPE_DIR}/old
mkdir -p ${EXPE_DIR}/new

# generate a robin file for each instance
BATCMD_BASE="batsim -p '${EXPE_DIR}/cluster.xml' -w '${EXPE_DIR}/kth_month.json' --mmax-workload"
robin generate "${EXPE_DIR}/old.yaml" \
      --output-dir "${EXPE_DIR}/old" \
      --batcmd "valgrind --tool=massif --time-unit=ms --massif-out-file='${EXPE_DIR}/old/massif.out' ${BATCMD_BASE} -e '${EXPE_DIR}/old/out_'" \
      --schedcmd "batsched -v easy_bf_fast"

robin generate "${EXPE_DIR}/new.yaml" \
      --output-dir "${EXPE_DIR}/new" \
      --batcmd "valgrind --tool=massif --time-unit=ms --massif-out-file='${EXPE_DIR}/new/massif.out' ${BATCMD_BASE} -e '${EXPE_DIR}/new/out_'" \
      --schedcmd "batsched -v easy_bf_fast"

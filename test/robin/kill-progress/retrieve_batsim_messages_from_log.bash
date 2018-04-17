#!/usr/bin/env bash
set -eux
BATPREFIX=$1
OUTDIR=$(echo ${BATPREFIX} | sed 'sW/outWWg')
BATLOG="${OUTDIR}/log/batsim.log"

cat ${BATLOG} | \grep -o "Sending '.*'" | \cut -d "'" -f2 | \
    \grep 'JOB_KILLED' > ${OUTDIR}/messages.txt

#!/usr/bin/env bash

# Define where Batsim should be cloned.
export BATSIM_DIR=/tmp/batsim

# Clone Batsim's latest release.
git clone --single-branch -b v2.0.0 https://framagit.org/batsim/batsim.git ${BATSIM_DIR}

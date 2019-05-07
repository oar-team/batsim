#!/usr/bin/env bash
set -eu

CI_DIR=$(realpath $(dirname $(realpath $0)))
# list nix dependencies from within the build environment, then push them to the external binary cache
nix-shell --pure $CI_DIR --arg useClang false --command ${CI_DIR}/list-store-paths-for-cachix.bash | cachix push batsim
# (re)build up-to-date batsim and test dependancies with gcc
nix-build $CI_DIR --arg useClang false

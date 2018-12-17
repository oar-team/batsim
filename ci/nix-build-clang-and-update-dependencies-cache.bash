#!/usr/bin/env bash
set -eu

CI_DIR=$(realpath $(dirname $(realpath $0)))
# (re)build up-to-date batsim and test dependancies with Clang
nix-build $CI_DIR --arg useClang true | cachix push batsim

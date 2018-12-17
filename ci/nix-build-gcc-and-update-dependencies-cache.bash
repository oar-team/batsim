#!/usr/bin/env bash
set -eu

CI_DIR=$(realpath $(dirname $(realpath $0)))
# (re)build up-to-date CI batsim package, push it on binary cache
nix-build $CI_DIR --arg useClang false | cachix push batsim

#!/usr/bin/env bash
set -eu

# (re)build up-to-date CI batsim package, push it on binary cache
nix build -f ci/default.nix | cachix push batsim

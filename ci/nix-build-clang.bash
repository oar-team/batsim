#!/usr/bin/env bash
set -eu

# (re)build up-to-date batsim and test dependancies with Clang
nix build -f ./ci/default.nix --arg kapack "import ~/Projects/kapack {useClang = true;}"

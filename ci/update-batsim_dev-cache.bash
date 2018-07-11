#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eu

# Build up-to-date batsim_dev package, push it on binary cache
nix-build ${DATAMOVEPKGS:-~/datamovepkgs} -A batsim_dev | cachix push batsim

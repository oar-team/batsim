#!/usr/bin/env bash

# Install cachix (using Nix).
# (up-to-date instructions are there: https://cachix.org/)
nix-env -iA cachix -f https://cachix.org/api/v1/install

# Configure cachix: tell Nix to use batsim.cachix.org as a binary cache.
cachix use batsim

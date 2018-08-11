#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eux

# Print which batsim is executed
echo "batsim realpath: $(realpath $(which batsim))"

# Execute the unit tests
./build/batsim --unittest

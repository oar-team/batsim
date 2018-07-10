#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eux

# Execute the unit tests
./build/batsim --unittest

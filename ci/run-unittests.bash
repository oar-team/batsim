#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eux

# Print which batsim is executed
echo "batsim realpath: $(realpath $(which batsim))"

# Execute the unit tests
BUILD_DIR=$(realpath $(dirname $(realpath $0))/../build)
$BUILD_DIR/batsim --unittest

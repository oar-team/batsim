#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eux

BUILD_DIR=$(realpath $(dirname $(realpath $0))/../build)

# Add built batsim in PATH
export PATH=${BUILD_DIR}:${PATH}

# Print which batsim is executed
echo "batsim realpath: $(realpath $(which batsim))"

# Execute the unit tests
batsim --unittest

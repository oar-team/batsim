#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix

# TODO: manage redis-server here

# Add built batsim in PATH
export PATH=$(realpath ./build):${PATH}

# Execute the tests
cd build
ctest

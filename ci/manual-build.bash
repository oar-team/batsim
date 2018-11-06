#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eux

# Start from a clean build directory
rm -rf ./build
mkdir -p ./build

# Usual cmake build stuff
cd ./build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -Denable_warnings=ON \
    -Dtreat_warnings_as_errors=OFF
make -j $(nproc)

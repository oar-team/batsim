#!/usr/bin/env bash

# install cachix (using Nix)
nix-env -iA cachix -f https://github.com/NixOS/nixpkgs/tarball/1d4de0d552ae9aa66a5b8dee5fb0650a4372d148

# configure cachix: tell it to use batsim.cachix.org
cachix use batsim

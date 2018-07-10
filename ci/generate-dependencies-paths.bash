#!/usr/bin/env nix-shell
#! nix-shell --pure -i bash ./default.nix
set -eu

# Remove previously generated dependencies
rm -f .nixdeps

# Retrieve the paths of the dependencies of the current shell
nix-env --query --out-path \
    | sed -n -E 'sW^.*(/nix/store/[a-zA-Z0-9]{32}-[^ ]+).*$W\1Wp' \
    > .nixdeps

# Print info
printf "Generated dependencies in .nixdeps:\n$(cat .nixdeps)\n"

#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eu

# Avoid pushing everything on the binary cache
excluder='texlive|biber|pdftk|qpdf|pdfdiff|gitflow|cachix|nox|nix-2|dia|asymptote|direnv|yamldiff'
deps=$(cat .nixdeps | grep -E -v ${excluder})
printf "About to push paths:\n${deps}\n\n"

# Push non-excluded paths to the remote cache
cachix push batsim $(echo ${deps} | tr '\n' ' ')

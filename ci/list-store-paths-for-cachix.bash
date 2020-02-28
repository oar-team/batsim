#!/usr/bin/env bash
env | tr '= :' '\n' | sed -E -n 'sW.*(/nix/store/[a-z0-9]{32}-)W\1Wp' | cut -d '/' -f1-4 | sort -u

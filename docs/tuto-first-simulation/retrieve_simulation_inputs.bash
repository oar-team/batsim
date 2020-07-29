#!/usr/bin/env bash

# Download a tarball of Batsim's latest release.
curl --output /tmp/batsim-v3.1.0.tar.gz \
    https://framagit.org/batsim/batsim/-/archive/v3.1.0/batsim-v3.1.0.tar.gz

# Extract tarball to /tmp/batsim-v3.1.0.
(cd /tmp && tar -xf batsim-v3.1.0.tar.gz)

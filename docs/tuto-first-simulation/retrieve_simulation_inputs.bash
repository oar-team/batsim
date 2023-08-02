#!/usr/bin/env bash

# Download a tarball of Batsim's latest release.
batversion='4.2.0'
curl --output "/tmp/batsim-v${batversion}.tar.gz" \
    "https://framagit.org/batsim/batsim/-/archive/v${batversion}/batsim-v${batversion}.tar.gz"

# Extract tarball to /tmp/batsim-src-stable.
(cd /tmp && tar -xf "batsim-v${batversion}.tar.gz" && mv "batsim-v${batversion}" batsim-src-stable)

# Base image with Nix installed: https://github.com/LnL7/nix-docker
FROM lnl7/nix:2018-04-17

# Survival kit
RUN nix-env -i git gnutar gzip

# Retrieve datamove nix packages
RUN git clone https://gitlab.inria.fr/vreis/datamove-nix.git /datamovepkgs

# Clone batsim repo
RUN git clone https://github.com/oar-team/batsim.git /batsim

# Pre build all test dependencies
RUN nix-build /datamovepkgs -A batsim_dev \
              --command "echo Dependencies pre-building done"

# CI dependencies
RUN nix-env -i ssh-ident rsync

# Base image with Nix installed: https://github.com/LnL7/nix-docker
FROM lnl7/nix:1.11.16

# Survival kit
RUN nix-env -i git gnutar gzip

# Clone batsim repo
RUN git clone https://github.com/oar-team/batsim.git /batsim

# Pre install all dependencies
RUN nix-shell /batsim/shell.nix \
              --command "echo Preinstalling dependencies done"

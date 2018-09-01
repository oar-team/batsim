# Base image with Nix installed: https://github.com/LnL7/nix-docker
FROM lnl7/nix:2018-04-17

# Survival kit
RUN nix-env -i git gnutar gzip gnused

#########################
# Cachix (binary cache) #
#########################
# Network setup
RUN nix-env -i iana-etc
RUN export IANA_PATH=$(nix-env -q --out-path | \
                       sed -n -E 'sWiana-etc-\S+\s+(.*)W\1Wp'); \
    cp ${IANA_PATH}/etc/protocols /etc/; \
    cp ${IANA_PATH}/etc/services /etc/

RUN nix-env -i nss-cacert
RUN export CACERT_PATH=$(nix-env -q --out-path | \
                       sed -n -E 'sWnss-cacert-\S+\s+(.*)W\1Wp'); \
    cp -r ${CACERT_PATH}/etc/ssl /etc/

# Cachix
RUN nix-env -if https://github.com/cachix/cachix/tarball/master \
    --substituters https://cachix.cachix.org \
    --trusted-public-keys \
        cachix.cachix.org-1:eWNHQldwUO7G2VkjpnjDbWwy4KQ/HNxht7H4SSoMckM=
ENV USER=root
RUN cachix use batsim

##########
# Batsim #
##########

# Retrieve datamove nix packages
ENV DATAMOVEPKGS=/datamovepkgs
ENV KAPACK=/kapack
RUN git clone https://github.com/oar-team/kapack.git ${KAPACK}
RUN ln -s ${KAPACK} ${DATAMOVEPKGS} # for backward compatibility

# Clone batsim repo
ENV BATSIM_DIR=/batsim
RUN git clone https://github.com/oar-team/batsim.git ${BATSIM_DIR}

# Pre build all test dependencies
RUN nix-build ${BATSIM_DIR}/ci \
              --command "echo Dependencies pre-building done"

# Pre build batsim_dev
RUN nix-shell ${DATAMOVEPKGS} -A batsim_dev

# CI dependencies
RUN nix-env -i openssh rsync netcat-gnu psmisc

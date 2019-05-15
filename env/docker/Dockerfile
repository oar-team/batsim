FROM nixos/nix:2.2.1
RUN nix-channel --remove nixpkgs \
 && nix-channel --add https://nixos.org/channels/nixos-19.03 nixpkgs \
 && nix-channel --update \
 && nix-env -f '<nixpkgs>' -iA \
      bash \
      curl \
      findutils \
      gitMinimal \
      glibc \
      gnugrep \
      gnused \
      gnutar \
      gzip \
      which \
      xz \
      openssh \
      rsync \
      netcat-gnu \
      psmisc \
 && nix-env -if https://github.com/cachix/cachix/tarball/master \
      --substituters https://cachix.cachix.org \
      --trusted-public-keys \
      cachix.cachix.org-1:eWNHQldwUO7G2VkjpnjDbWwy4KQ/HNxht7H4SSoMckM= \
 && cachix use batsim \
 && nix-collect-garbage -d \
 && ln -s $(realpath $(which bash)) /bin/bash

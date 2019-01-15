FROM oarteam/nix:2018-09-21
RUN nix-env -f '<nixpkgs>' -iA \
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
 && nix-collect-garbage -d

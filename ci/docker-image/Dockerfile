FROM lnl7/nix:2.1.2

RUN nix-env -iA \
 nixpkgs.gnutar \
 nixpkgs.gitMinimal \
 nixpkgs.gzip \
 nixpkgs.iana-etc \
 nixpkgs.cachix \ 
 && \
 nix-collect-garbage -d

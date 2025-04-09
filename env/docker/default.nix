{ pkgs ? import (fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/21.05.tar.gz";
    sha256 = "1ckzhh24mgz6jd1xhfgx0i9mijk6xjqxwsshnvq789xsavrmsc36";
  }) {}
}:

let
  self = rec {
    nix-image = pkgs.dockerTools.pullImage {
      imageName = "nixpkgs/cachix";
      imageDigest = "sha256:fe785cb65b7bcd332154183e4509ef9b6aeb9913d5b13fb6312df6ee9cc8a543";
      sha256 = "10jb3l46zz5vvpfddc24ppp0cibgdgkxi33n459dr5b7h0fhk51v";
      os = "linux";
      arch = "amd64";
    };
    batsim-ci-image = pkgs.dockerTools.buildImage {
      name = "oarteam/batsim_ci";
      tag = "latest";
      created = "now";
      fromImage = nix-image;
      contents = [
        # - enable `nix` and `nix flakes` commands
        # - enable pull from batsim/capack cachix binary caches
        (pkgs.writeTextFile {
          name = "nix.conf";
          destination = "/etc/nix/nix.conf";
          text = ''
            experimental-features = nix-command flakes
            substituters = https://cache.nixos.org https://batsim.cachix.org https://capack.cachix.org
            trusted-public-keys = cache.nixos.org-1:6NCHdD59X431o0gWypbMrAURkbJ16ZPMQFGspcDShjY= batsim.cachix.org-1:zO3Z03P62i9HM4gRRgycYeV7mYtIwEoLv+XpAr1Nu7I= capack.cachix.org-1:38D+QFk3JXvMYJuhSaZ+3Nm/Qh+bZJdCrdu4pkIh5BU=
          '';
        })
      ];
      # required by gitlab-ci runner
      config.Entrypoint = ["/bin/bash" "-c"];
    };
  };
in
  self

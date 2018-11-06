{ doCheck ? false,
  #pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {},
  kapack ?import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  #{ inherit pkgs; }
  {}
}:

let
  inherit (kapack) batsim_dev;
  inherit (kapack.pkgs) nix which coreutils;
in
(batsim_dev.override {installTestsDeps = true;
                      installDocDeps=true;}).overrideAttrs (attrs: rec {
    name = "batsim-ci";
    src = ../.;
    enableParallelBuilding = true;
    inherit doCheck;

    nativeBuildInputs = attrs.nativeBuildInputs ++ [nix which coreutils];
})

let
  pkgs = import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {};
  dmpkgs = import
    ( fetchTarball "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz")
  { inherit pkgs; };
in

with dmpkgs;
with pkgs;

(batsim_dev.override {}).overrideAttrs (attrs: rec {
    name = "batsim-ci";
    src = ../.;
    enableParallelBuilding = true;
    doCheck = false;

    nativeBuildInputs = attrs.nativeBuildInputs ++ [nix];
})

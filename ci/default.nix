let
  pkgs = import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {};
  kapack = import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  { inherit pkgs; };
in

with kapack;
with pkgs;

(batsim_dev.override {installTestsDeps = true;
                      installDocDeps=true;}).overrideAttrs (attrs: rec {
    name = "batsim-ci";
    src = ../.;
    enableParallelBuilding = true;
    doCheck = false;

    nativeBuildInputs = attrs.nativeBuildInputs ++ [nix which coreutils];
})

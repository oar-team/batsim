with import (
    fetchTarball
    "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz")
  { };
with import <nixpkgs> {};

(batsim_dev.override { batsim = (batsim.override { simgrid = simgrid_dev; }); }).overrideAttrs (attrs: rec {
  version = "dev_local";
  src = ./.;
  buildInputs = attrs.nativeBuildInputs;
})

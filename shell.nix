with import (
    fetchTarball
    "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz")
    { };
# pkgs = (import <nixpkgs> {});
#with import <nixpkgs> {};

#let
#  simgrid_local = (pkgs.simgrid_dev.override {
#  debug = true;
#  # fortranSupport = true;
#  # buildDocumentation = true;
#  # buildJavaBindings = true;
#  # modelCheckingSupport = true;
#  #  moreTests = true;
#  }).overrideAttrs (attrs: {
#  src = /home/mmercier/Projects/simgrid;
#  dontStrip = true;
#  doCheck = false;
#});
#in
(batsim_dev.override {
  #batsim = (batsim.override { simgrid = simgrid_local; });
  batsim = (batsim.override { simgrid = simgrid_dev; });
}).overrideAttrs (attrs: rec {
  version = "dev_local";
  src = ./.;
  buildInputs = attrs.nativeBuildInputs;
  makeFlags = ["VERBOSE=1"];
  doCheck = false;
})

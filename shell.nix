with import (
    fetchTarball
    "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz")
    { };

batsim_dev

#let
#  simgrid_local = simgrid_dev.overrideAttrs (attrs: {
#       src = /path/to/simgrid;
#       dontStrip = true;
#       doCheck = false;
#     }
#   );
#in
#(
#  batsim_dev.override { simgrid = simgrid_local; }
#).overrideAttrs (attrs: rec {
#  version = "dev_local";
#  src = ./.;
#  makeFlags = ["VERBOSE=1"];
#  doCheck = false;
#})

let
  pkgs = import (     fetchTarball "https://github.com/NixOS/nixpkgs/archive/17.09.tar.gz") {};
  dmpkgs = import
    #~/Projects/datamove-nix
    ( fetchTarball "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz")
  { inherit pkgs; };
in
# To use the batsim_dev package build environment
#
#dmpkgs.batsim_dev

# To use local simgrid or a simgrid fork use the folowing instead

with dmpkgs;
with pkgs;
let
  simgrid_local = simgrid_dev.overrideAttrs (attrs: {
    src = ~/Projects/simgrid;
    #src = fetchFromGitHub {
    #  owner = "mpoquet";
    #  repo = "simgrid";
    #  rev = "d6d2d67";
    #  sha256 = "00m4a57jf8mrcjn5nld4bdwbnak7rff37bm7ph9c1xmywg02yccy";
    #};
  });
in
(
  batsim_dev.override { simgrid = simgrid_local; }
).overrideAttrs (attrs: rec {
#batsim_dev.overrideAttrs (attrs: rec {
  version = "dev_local";
  src = ./.;
  makeFlags = ["VERBOSE=1" "-g"];
})

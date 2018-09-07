let
  pkgs = import
    ( fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {};
  kapack = import
    #~/Projects/kapack
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  { inherit pkgs; };
in
# To use the batsim_dev package build environment
#
#kapack.batsim_dev

# To use local simgrid or a simgrid fork use the folowing instead

with kapack;
with pkgs;
#let
#  simgrid_local = simgrid_dev.overrideAttrs (attrs: {
#
#    # Use local project
#    #
#    #src = ~/Projects/simgrid;
#
#    # Use a fork
#    #
#    #src = fetchFromGitHub {
#    #  owner = "mpoquet";
#    #  repo = "simgrid";
#    #  rev = "d6d2d67";
#    #  sha256 = "00m4a57jf8mrcjn5nld4bdwbnak7rff37bm7ph9c1xmywg02yccy";
#    #};
#
#    # Use specific version of simgrid
#    #
#    src = fetchFromGitHub {
#      owner = "simgrid";
#      repo = "simgrid";
#      rev = "b1801e70";
#      sha256 = "0lxmhyapbx252z194pxmydx67cc8hz3p1s5pw28d9x0yip0f9sv2";
#    };
#  });
#in
#(
#  batsim_dev.override { simgrid = simgrid_local; }
#).overrideAttrs (attrs: rec {
batsim_dev.overrideAttrs (attrs: rec {
  version = "dev_local";
  src = ./.;
  makeFlags = ["VERBOSE=1"];
})

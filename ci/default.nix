# The value of the following arguments can be overridden at call time:
# - nix-shell ci/default.nix --arg useClang true
# - nix-build ci/default.nix --arg useClang false
{ doCheck ? false,  # run batsim tests when building batsim-ci derivation
  useClang ? true,  # use clang instead of gcc to build batsim
  kapack ? import
    (fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  {}
}:

let
  inherit (kapack) batsim_dev;
  inherit (kapack.pkgs) nix which coreutils;
in
(batsim_dev.override (
  { installTestsDeps = true; } //
  kapack.pkgs.stdenv.lib.optionalAttrs useClang { stdenv = kapack.pkgs.clangStdenv; }
)).overrideAttrs (attrs: rec {
    name = "batsim-ci";
    src = ../.;
    enableParallelBuilding = true;
    inherit doCheck;

    nativeBuildInputs = attrs.nativeBuildInputs ++ [nix which coreutils];
})

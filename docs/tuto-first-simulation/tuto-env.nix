let
  pkgs = import
    (fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {};
  kapack = import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz") {};
in

pkgs.stdenv.mkDerivation rec {
  name = "tuto-env";
  env = pkgs.buildEnv { name = name; paths = buildInputs; };
  buildInputs = [
    kapack.batsim # simulator
    kapack.batsched # scheduler
    kapack.batexpe # experiment management tools
  ];
}

let
  kapack = import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz") {};
in

kapack.pkgs.stdenv.mkDerivation rec {
  name = "tuto-env";
  env = kapack.pkgs.buildEnv { name = name; paths = buildInputs; };
  buildInputs = [
    kapack.batsim # simulator
    kapack.batsched # scheduler
    kapack.batexpe # experiment management tools
  ];
}

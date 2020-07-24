let
  kapack = import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz") {};
in

kapack.pkgs.stdenv.mkDerivation rec {
  name = "tuto-env-dev";
  env = kapack.pkgs.buildEnv { name = name; paths = buildInputs; };
  buildInputs = [
    kapack.batsim_dev # simulator
    kapack.batsched_dev # scheduler
    kapack.batexpe_dev # experiment management tools
  ];
}

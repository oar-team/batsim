let
  kapack = import
    ( fetchTarball "https://github.com/oar-team/nur-kapack/archive/master.tar.gz") {};
in

kapack.pkgs.mkShell rec {
  name = "tuto-env";
  buildInputs = [
    kapack.batsim # simulator
    kapack.batsched # scheduler
    kapack.batexpe # experiment management tools
    kapack.pkgs.curl # used to retrieve batsim workloads/platforms
  ];
}

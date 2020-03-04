let
  kapack = import
    ( fetchTarball "https://github.com/oar-team/nur-kapack/archive/master.tar.gz") {};
in

kapack.pkgs.mkShell rec {
  name = "tuto-env-master";
  buildInputs = [
    kapack.batsim-master # simulator
    kapack.batsched-master # scheduler
    kapack.batexpe-master # experiment management tools
  ];
}

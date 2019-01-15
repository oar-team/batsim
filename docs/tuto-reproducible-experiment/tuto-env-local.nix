let
  pkgs = import
    (fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {};
  kapack = import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  { inherit pkgs; };
in

pkgs.stdenv.mkDerivation rec {
  name = "tuto-env";
  env = pkgs.buildEnv { name = name; paths = buildInputs; };
  buildInputs = [
    kapack.batsim # simulator
    kapack.batexpe # experiment management tools

    # pybatsim schedulers
    (kapack.pybatsim.overrideAttrs (oldAttrs: rec {
      src = /path/to/pybatsim; # <--------------------------- Change path here
    }))
  ];
}

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
      src = pkgs.fetchgit {
        url = "https://gitlab.inria.fr/batsim/pybatsim.git";
        rev = "9b7032b670442d8026b70f3527d01222ab0b4f74";
        sha256 = "0dl44rsbgacr2kh4q3y7ra243c8a9xrvay41f0csg3sz983w48cj";
      };
    }))
  ];
}

{ doCheck ? false,
  kapack ? import
    (fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  {}
}:

let
  inherit (kapack) batsim_dev;
  inherit (kapack.pkgs) nix which coreutils;
in
(batsim_dev.override {installTestsDeps = true;
                      installDocDeps=true;}).overrideAttrs (attrs: rec {
    name = "batsim-ci";
    src = ../.;
    enableParallelBuilding = true;
    inherit doCheck;

    nativeBuildInputs = attrs.nativeBuildInputs ++ [nix which coreutils];
})

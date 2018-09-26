let
  pkgs = import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/18.03.tar.gz") {};
in

with pkgs;

stdenv.mkDerivation rec {
  name = "rst-shell";

  src = ./.;

  nativeBuildInputs = [
    python36Packages.sphinx
    python36Packages.sphinx_rtd_theme
  ];
}

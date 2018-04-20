{
  pkgs ? import (
    fetchTarball "https://github.com/NixOS/nixpkgs/archive/17.09.tar.gz") {},
  dmpkgs ? import (
    #fetchTarball "https://gitlab.inria.fr/vreis/datamove-nix/repository/35d8902a19f48403b287da9c0a46b92090997ac8/archive.tar.gz"
    fetchTarball "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz"
    ) { inherit pkgs;},
}:

pkgs.stdenv.mkDerivation rec {
  name = "batsim-test-env";
  env = pkgs.buildEnv { name = name; paths = buildInputs; };

  simgrid_batsim = dmpkgs.simgrid_dev.overrideDerivation(attrs: rec {
    src = pkgs.fetchgit {
      url = "https://github.com/simgrid/simgrid.git";
      rev = "54f07f038966ffe0b3e8ef526e9192f7eb49ad09";
      sha256 = "0afsh7ys4lcbdabaqzc2m6ddar875f3rrmzjdb00k9x2pr91whpa";
    };
    doCheck = false;
  });

  buildInputs = [
    #########
    # Build #
    #########
    pkgs.gcc
    pkgs.clang
    pkgs.boost
    pkgs.gmp
    pkgs.rapidjson
    pkgs.openssl
    pkgs.hiredis
    pkgs.libev
    pkgs.cppzmq
    pkgs.zeromq
    pkgs.cmake
    simgrid_batsim
    dmpkgs.redox

    ########
    # Test #
    ########
    # Run simulations
    dmpkgs.batexpe
    dmpkgs.batsched_dev
    pkgs.redis
    # Check results
    pkgs.python36
    pkgs.python36Packages.pandas
    # Misc
    pkgs.doxygen
  ];
}

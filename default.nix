{
  pkgs ? import <nixpkgs> {},
  datamove ? import (fetchTarball  "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz") {}
}:
{
  batsim_test = datamove.batsim.overrideDerivation (attrs: rec {
    version = "dev";
    src = ./.;
    preConfigure = "rm -rf ./build/*";
    expeToolInputs = with pkgs.python36Packages; [
      async-timeout
      datamove.coloredlogs
      pandas
      pyyaml
      datamove.execo
    ];
    testInputs = with pkgs; [
      (python36.withPackages (ps: [ datamove.pybatsim ]))
      datamove.batsched
      redis
      R
      rPackages.ggplot2
      rPackages.dplyr
      rPackages.scales
      psmisc # for pstree
      iproute # for ss
    ];
    docInputs = with pkgs; [
      doxygen
      graphviz
    ];
    buildInputs =
      attrs.buildInputs ++ expeToolInputs ++ testInputs ++
      docInputs ;
    doCheck = true;
    preCheck = ''
      # Patch tests script she bang
      patchShebangs ..

      # Patch inside scripts in the yaml test files that are not catch by
      # patchShebangs utility
      find .. -type f | xargs sed -i -e 's#\(.*\)/usr/bin/env\(.*\)#\1${pkgs.coreutils}/bin/env\2#'

      # start Redis and keep PID
      redis-server > /dev/null &
      REDIS_PID=$!
    '';
    checkPhase = ''
      runHook preCheck

      PATH="$(pwd):$PATH" ctest --output-on-failure -E 'remote'

      runHook postCheck
      '';
    postCheck = ''
      kill $REDIS_PID
    '';
  });
}
#in
#(pkgs.buildFHSUserEnv {
#  name = "batsim-test";
#  targetPkgs = pkgs: with datamove; [
#    batsim_dev
#    pybatsim
#  ];
#  runScript = "bash";
#}).env

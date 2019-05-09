{ kapack ? import
    (fetchTarball "https://github.com/oar-team/kapack/archive/master.tar.gz")
  {}
}:

let
  pkgs = kapack.pkgs;
  jobs = rec {
    batsim = kapack.batsim.overrideAttrs (attr: rec {
      src = pkgs.lib.sourceByRegex ./. [
        "^src"
        "^src/.*\.?pp"
        "^src/unittest"
        "^src/unittest/.*\.?pp"
        "^CMakeLists\.txt"
        "^cmake"
        "^cmake/Modules"
        "^cmake/Modules/.*\.cmake.*"
        "^VERSION"
      ];
    });
    integration_tests = pkgs.stdenv.mkDerivation rec {
      name = "batsim-integration-tests";
      src = pkgs.lib.sourceByRegex ./. [
        "^test"
        "^test/.*\.py"
        "^platforms"
        "^platforms/.*\.xml"
        "^workloads"
        "^workloads/.*\.json"
        "^workloads/smpi"
        "^workloads/smpi/.*"
        "^workloads/smpi/.*/.*\.txt"
      ];
      buildInputs = with kapack.pythonPackages; [
        batsim kapack.batsched_dev kapack.batexpe pkgs.redis
        pytest pandas];
      buildPhase = "pytest test/ > pytest.log";
      installPhase = ''
        mkdir -p $out
        mv pytest.log $out/
      '';
    };
    doxydoc = pkgs.stdenv.mkDerivation rec {
      name = "batsim-doxygen-documentation";
      src = pkgs.lib.sourceByRegex ./. [
        "^src"
        "^src/.*\.?pp"
        "^doc"
        "^doc/Doxyfile"
        "^doc/doxygen_mainpage.md"
      ];
      buildInputs = [pkgs.doxygen];
      buildPhase = "(cd doc && doxygen)";
      installPhase = ''
        mkdir -p $out
        mv doc/doxygen_doc/html/* $out/
      '';
      checkPhase = ''
        nb_warnings=$(cat doc/doxygen_warnings.log | wc -l)
        if [[ $nb_warnings -gt 0 ]] ; then
          echo "FAILURE: There are doxygen warnings!"
          cat doc/doxygen_warnings.log
          exit 1
        fi
      '';
      doCheck = true;
    };
  };
in
  jobs

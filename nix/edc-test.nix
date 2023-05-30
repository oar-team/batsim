{ stdenv, lib
, batsim, batsim-edc-libs, pytest, pytest-html
, doCoverage ? false
, failOnTestFailed ? true
}:

assert lib.assertMsg (batsim.hasInternalTestBinaries) "batsim has not been compiled with internal tests enabled";
assert lib.assertMsg (!doCoverage || batsim.hasCoverage) "batsim has not been compiled with coverage while internal tests should do coverage";

stdenv.mkDerivation rec {
  name = "batsim-edc-test";

  buildInputs = [
    batsim
    batsim-edc-libs
    pytest
    pytest-html
  ];

  src = lib.sourceByRegex ../. [
    "^test"
    "^test/.*\.py"
    "^platforms"
    "^platforms/.*\.xml"
    "^workloads"
    "^workloads/.*\.json"
    "^workloads/.*\.dax"
    "^workloads/smpi"
    "^workloads/smpi/.*"
    "^workloads/smpi/.*/.*\.txt"
    "^events"
    "^events/.*\.txt"
  ];

  # set environment variables read at runtime by the pytest test suite
  EDC_LD_LIBRARY_PATH = "${batsim-edc-libs}/lib";
  PLATFORM_DIR = "${src}/platforms";
  WORKLOAD_DIR = "${src}/workloads";

  pytestArgs = "--html=./report/pytest_report.html --junit-xml=./report/pytest_report.xml";
  preBuild = lib.optionalString doCoverage ''
    mkdir -p gcda
    export GCOV_PREFIX=$(realpath gcda)
    export GCOV_PREFIX_STRIP=${batsim.GCOV_PREFIX_STRIP}
  '';

  buildPhase = ''
    runHook preBuild

    # do not force the derivation build to fail if some tests have failed
    set +e
    pytest ${pytestArgs}
    pytest_returncode=$?
    echo $pytest_returncode > ./pytest_returncode
    set -e

    echo "pytest return code: $pytest_returncode"
  '' + lib.optionalString failOnTestFailed ''
    if [ $pytest_returncode -ne 0 ] ; then
      exit 1
    fi
  '';

  installPhase = ''
    mkdir -p $out
    cp -r report $out/
  '' + lib.optionalString doCoverage ''
    mv gcda $out/
  '';
}

{ stdenv, lib
, batsim, batsim-edc-libs, batsim-internal-test
, pytest, pytest-html, pandas
, doCoverage ? false
, failOnTestFailed ? true
, startFromInternalCoverage ? false
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
    pandas
  ] ++ lib.optional startFromInternalCoverage [ batsim-internal-test ];

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
    "^workloads/usage-trace"
    "^workloads/usage-trace/from-real-trace"
    "^workloads/usage-trace/from-real-trace/.*\.txt"
    "^workloads/usage-trace/hetero-onephase-10-twophases-1-97"
    "^workloads/usage-trace/hetero-onephase-10-twophases-1-97/.*\.txt"
    "^workloads/usage-trace/hetero-onephase-20-60"
    "^workloads/usage-trace/hetero-onephase-20-60/.*\.txt"
    "^workloads/usage-trace/homo-onephase-100"
    "^workloads/usage-trace/homo-onephase-100/.*\.txt"
    "^workloads/usage-trace/homo-onephase-50"
    "^workloads/usage-trace/homo-onephase-50/.*\.txt"
    "^workloads/usage-trace/homo-twophases-100-10"
    "^workloads/usage-trace/homo-twophases-100-10/.*\.txt"
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
  '' + lib.optionalString startFromInternalCoverage ''
    cp --no-preserve=all ${batsim-internal-test}/gcda/*.gcda gcda/
  '';

  buildPhase = ''
    runHook preBuild

    mkdir -p $out

    # do not force the derivation build to fail if some tests have failed
    set +e
    pytest ${pytestArgs} --test-output-dir=$out/test-out
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
    cp ./pytest_returncode $out/
    cp -r report $out/
  '' + lib.optionalString doCoverage ''
    mv gcda $out/
  '';
}

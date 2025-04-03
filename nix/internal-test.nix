{ stdenv, lib
, batsim
, doCoverage ? false
}:

assert lib.assertMsg (batsim.hasInternalTestBinaries) "batsim has not been compiled with internal tests enabled";
assert lib.assertMsg (!doCoverage || batsim.hasCoverage) "batsim has not been compiled with coverage while internal tests should do coverage";

stdenv.mkDerivation rec {
  name = "batsim-internal-test";

  buildInputs = [
    batsim
  ];

  unpackPhase = "true"; # no src for this package
  buildPhase = lib.optionalString doCoverage ''
    mkdir -p gcda
    export GCOV_PREFIX=$(realpath gcda)
    export GCOV_PREFIX_STRIP=${batsim.GCOV_PREFIX_STRIP}
  '' + ''
    batsim-func-tests --gtest_output="json:./test_report.json"
  '';
  installPhase = ''
    mkdir -p $out
    cp ./test_report.* $out/
  '' + lib.optionalString doCoverage ''
    mv gcda $out/
  '';
}

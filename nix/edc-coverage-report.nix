{ stdenv, lib
, batsim, batsim-edc-test
, gcovr
, coverageCobertura ? true
, coverageCoveralls ? true
, coverageGcovTxt ? true
, coverageHtml ? true
, coverageSonarqube ? true
}:

stdenv.mkDerivation rec {
  name = "coverage-report";

  buildInputs = batsim.buildInputs ++
    [ gcovr ] ++
    [ batsim batsim-edc-test ];
  src = batsim.src;

  buildPhase = ''
    mkdir cov-merged
    cd cov-merged
    cp ${batsim}/gcno/* ${batsim-edc-test}/gcda/* ./
    gcov -p *.gcno
    mkdir report
  '' + lib.optionalString coverageHtml ''
    mkdir -p report/html
  '' + lib.optionalString coverageGcovTxt ''
    mkdir -p report/gcov-txt
    cp \^\#src\#*.gcov report/gcov-txt/
  '' + ''
    gcovr -g -k -r .. --filter '\.\./src/' \
      --txt report/file-summary.txt \
      --csv report/file-summary.csv \
      --json-summary report/file-summary.json \
    '' + lib.optionalString coverageCobertura ''
      --xml report/cobertura.xml \
    '' + lib.optionalString coverageCoveralls ''
      --coveralls report/coveralls.json \
    '' + lib.optionalString coverageHtml ''
      --html-details report/html/index.html \
    '' + lib.optionalString coverageSonarqube ''
      --sonarqube report/sonarqube.xml \
    '' + ''
      --print-summary
  '';
  installPhase = ''
    mkdir -p $out
    cp -r report/* $out/
  '';
}

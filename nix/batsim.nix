{ stdenv, lib
, cppMesonDevBase
, meson, ninja, pkg-config
, simgrid, intervalset, boost, rapidjson, zeromq, pugixml, batprotocol-cpp, cli11, gtest
, doInternalTests ? true
, debug ? false
, werror ? false
, doCoverage ? false
}:

(cppMesonDevBase {
  inherit stdenv lib meson ninja pkg-config debug werror doCoverage;
  coverageGcnoGlob = "batsim.p/*.gcno libbatlib.a.p/*.gcno";
}).overrideAttrs(attrs: rec {
  name = "batsim";

  src = lib.sourceByRegex ../. [
    "^meson\.build"
    "^meson_options\.txt"
    "^src"
    "^src/.*\.hpp"
    "^src/.*\.cpp"
    "^src/test"
    "^src/test/.*\.cpp"
    "^src/test/.*\.hpp"
  ];

  mesonFlags = attrs.mesonFlags
    ++ lib.optional doInternalTests [ "-Ddo_internal_tests=true" ];

  # runtimeDeps is used to generate multi-layered docker contained
  runtimeDeps = [
    simgrid
    intervalset
    rapidjson
    zeromq
    pugixml
    batprotocol-cpp
    cli11
  ];
  buildInputs = [
    boost
  ] ++ lib.optional doInternalTests [ gtest.dev ]
  ++ runtimeDeps;

  passthru = rec {
    hasInternalTestBinaries = doInternalTests;
    hasDebugSymbols = debug;
    hasCoverage = doCoverage;
    GCOV_PREFIX_STRIP = "5";
    DEBUG_SRC_DIRS = [ "${src}/src" ];
    GDB_DIR_ARGS = map (x: "--directory=" + x) DEBUG_SRC_DIRS;
  };
})

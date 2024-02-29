{ stdenv, lib
, cppMesonDevBase
, batprotocol-cpp, intervalset, nlohmann_json
, meson, ninja, pkg-config
, debug ? false
, werror ? false
}:

(cppMesonDevBase {
  inherit stdenv lib meson ninja pkg-config debug werror;
  doCoverage = false;
}).overrideAttrs(attrs: rec {
  name = "edc-libs";
  src = lib.sourceByRegex ../test/edc-lib [
    "^meson\.build"
    "^.*\.?pp"
    "^.*\.h"
  ];
  buildInputs = [
    batprotocol-cpp
    intervalset
    nlohmann_json
  ];
  passthru = rec {
    DEBUG_SRC_DIRS = intervalset.DEBUG_SRC_DIRS ++ batprotocol-cpp.DEBUG_SRC_DIRS ++ [ "${src}" ];
    GDB_DIR_ARGS = map (x: "--directory=" + x) DEBUG_SRC_DIRS;
  };
})

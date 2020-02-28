#!/usr/bin/env bash
function list_store_paths_for_cachix {
    var_path=$1
    echo $var_path | tr ' :' '\n' | sed -E -n 'sW(/nix/store/.*)/.*W\1Wp'
}

list_store_paths_for_cachix ${PKG_CONFIG_PATH}
list_store_paths_for_cachix ${PATH}
list_store_paths_for_cachix ${PYTHONPATH}

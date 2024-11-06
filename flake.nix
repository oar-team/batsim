{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=23.11";
    nixpkgs-pytest.url = "github:nixos/nixpkgs?ref=22.11";
    flake-utils.url = "github:numtide/flake-utils";
    nur-kapack = {
      url = "github:oar-team/nur-kapack";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
    };
    intervalset = {
      url = "git+https://framagit.org/batsim/intervalset";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.nur-kapack.follows = "nur-kapack";
      inputs.flake-utils.follows = "flake-utils";
    };
    batprotocol = {
      url = "git+https://framagit.org/batsim/batprotocol";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.nur-kapack.follows = "nur-kapack";
      inputs.flake-utils.follows = "flake-utils";
    };
  };

  outputs = { self, nixpkgs, nixpkgs-pytest, nur-kapack, intervalset, batprotocol, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        pkgs-pytest = import nixpkgs-pytest { inherit system; };
        kapack = nur-kapack.packages.${system};
        simgrid-base = kapack.simgrid;
        release-options = {
          debug = false;
          doCoverage = false;
          simgrid = simgrid-base.override { debug = false; };
          batprotocol-cpp = batprotocol.packages-release.${system}.batprotocol-cpp;
          intervalset = intervalset.packages-release.${system}.intervalset;
        };
        debug-options = {
          debug = true;
          doCoverage = false;
          simgrid = simgrid-base.override { debug = true; };
          batprotocol-cpp = batprotocol.packages-debug.${system}.batprotocol-cpp;
          intervalset = intervalset.packages-debug.${system}.intervalset;
        };
        debug-cov-options = debug-options // {
          doCoverage = true;
        };
        base-defs = {
          cppMesonDevBase = nur-kapack.lib.${system}.cppMesonDevBase;
          pytest = pkgs-pytest.python3Packages.pytest;
          pytest-html = pkgs-pytest.python3Packages.pytest-html;
          pandas = pkgs-pytest.python3Packages.pandas;
        };
        callPackage = mergedPkgs: deriv-func: attrset: options: pkgs.lib.callPackageWith(mergedPkgs // options) deriv-func attrset;
      in rec {
        functions = rec {
          batsim = import ./nix/batsim.nix;
          batsim-internal-test = import ./nix/internal-test.nix;
          batsim-edc-libs = import ./nix/edc-libs.nix;
          batsim-edc-test = import ./nix/edc-test.nix;
          batsim-coverage-report = import ./nix/coverage-report.nix;
          # batsim-sphinx-doc
          generate-packages = mergedPkgs: options: {
            batsim = callPackage mergedPkgs batsim {} options;
            batsim-internal-test = callPackage mergedPkgs batsim-internal-test {} options;
            batsim-edc-libs = callPackage mergedPkgs batsim-edc-libs {} options;
            batsim-edc-test = callPackage mergedPkgs batsim-edc-test {} options;
          };
        };
        packages-release = functions.generate-packages (pkgs // base-defs // packages-release) release-options;
        packages-debug = functions.generate-packages (pkgs // base-defs // packages-debug) debug-options;
        packages-debug-cov = functions.generate-packages (pkgs // base-defs // packages-debug-cov) debug-cov-options;
        packages = packages-release // rec {
          ci-batsim-werror-gcc = callPackage (pkgs // base-defs // packages-release) functions.batsim ({ stdenv = pkgs.gccStdenv; werror = true; }) release-options;
          ci-batsim-werror-clang = callPackage (pkgs // base-defs // packages-release) functions.batsim ({ stdenv = pkgs.clangStdenv; werror = true; }) release-options;
          ci-batsim-internal-test = packages-debug-cov.batsim-internal-test;
          ci-batsim-edc-test = packages-debug-cov.batsim-edc-test;
          ci-batsim-edc-test-from-internal-gcda = packages-debug-cov.batsim-edc-test.override { startFromInternalCoverage = true; failOnTestFailed = false; };
          ci-batsim-internal-test-coverage-report = callPackage pkgs functions.batsim-coverage-report { batsim = packages-debug-cov.batsim; batsim-test = packages-debug-cov.batsim-internal-test; } debug-cov-options;
          ci-batsim-edc-test-coverage-report = callPackage pkgs functions.batsim-coverage-report { batsim = packages-debug-cov.batsim; batsim-test = packages-debug-cov.batsim-edc-test; } debug-cov-options;
          ci-batsim-test-coverage-report = callPackage pkgs functions.batsim-coverage-report { batsim = packages-debug-cov.batsim; batsim-test = ci-batsim-edc-test-from-internal-gcda; } debug-cov-options;
        };
        devShells = {
          external-test = pkgs.mkShell rec {
            buildInputs = [
              packages-debug.batsim
              packages-debug.batsim-edc-libs
              pkgs-pytest.python3Packages.ipython
              pkgs-pytest.python3Packages.pandas
              pkgs-pytest.python3Packages.pytest
              pkgs-pytest.python3Packages.pytest-html
              pkgs.gdb
              pkgs.cgdb
            ];

            DEBUG_SRC_DIRS = packages-debug.batsim.DEBUG_SRC_DIRS ++ packages-debug.batsim-edc-libs.DEBUG_SRC_DIRS;
            GDB_DIR_ARGS = packages-debug.batsim.GDB_DIR_ARGS ++ packages-debug.batsim-edc-libs.GDB_DIR_ARGS;

            EDC_LD_LIBRARY_PATH = "${packages-release.batsim-edc-libs}/lib";
            shellHook = ''
              export PLATFORM_DIR=$(realpath platforms)
              export WORKLOAD_DIR=$(realpath workloads)

              echo Found debug_info source paths. ${builtins.concatStringsSep ":" DEBUG_SRC_DIRS}
              echo Run the following command to automatically load these directories to gdb.
              echo gdb \$\{GDB_DIR_ARGS\}
            '';
          };
        };
      }
    );
}

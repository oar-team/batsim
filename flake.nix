{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=22.11";
    flake-utils.url = "github:numtide/flake-utils";
    nur-kapack = {
      url = "github:oar-team/nur-kapack/master";
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

  outputs = { self, nixpkgs, nur-kapack, intervalset, batprotocol, flake-utils }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (system:
      let
        pkgs = import nixpkgs { inherit system; };
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
          pytest = pkgs.python3Packages.pytest;
          pytest-html = pkgs.python3Packages.pytest-html;
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
        packages = packages-release // {
          ci-batsim-werror-gcc = callPackage (pkgs) functions.batsim ({ stdenv = pkgs.gccStdenv; werror = true; } // base-defs) release-options;
          ci-batsim-werror-clang = callPackage (pkgs) functions.batsim ({ stdenv = pkgs.clangStdenv; werror = true; } // base-defs) release-options;
          ci-batsim-internal-test = packages-debug-cov.batsim-internal-test;
          ci-batsim-internal-test-coverage-report = callPackage pkgs functions.batsim-coverage-report { batsim = packages-debug-cov.batsim; batsim-test = packages-debug-cov.batsim-internal-test; } debug-cov-options;
          ci-batsim-edc-test = packages-debug-cov.batsim-edc-test;
          ci-batsim-edc-test-coverage-report = callPackage pkgs functions.batsim-coverage-report { batsim = packages-debug-cov.batsim; batsim-test = packages-debug-cov.batsim-edc-test; } debug-cov-options;
        };
        devShells = {
          external-test = pkgs.mkShell {
            buildInputs = [
              packages-release.batsim
              packages-release.batsim-edc-libs
              pkgs.python3Packages.ipython
              pkgs.python3Packages.pytest
            ];

            EDC_LD_LIBRARY_PATH = "${packages-release.batsim-edc-libs}/lib";
            shellHook = ''
              export PLATFORM_DIR=$(realpath platforms)
              export WORKLOAD_DIR=$(realpath workloads)
            '';
          };
        };
      }
    );
}

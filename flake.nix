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
        base-defs = { cppMesonDevBase = nur-kapack.lib.${system}.cppMesonDevBase; };
        callPackage = mergedPkgs: deriv-func: attrset: options: pkgs.lib.callPackageWith(mergedPkgs // options) deriv-func attrset;
      in rec {
        functions = rec {
          batsim = import ./nix/batsim.nix;
          batsim-internal-test = import ./nix/internal-test.nix;
          # batsim-edc-libs
          # batsim-edc-test
          batsim-coverage-report = import ./nix/coverage-report.nix;
          # batsim-sphinx-doc
          generate-packages = mergedPkgs: options: {
            batsim = callPackage mergedPkgs batsim {} options;
            batsim-internal-test = callPackage mergedPkgs batsim-internal-test {} options;
            batsim-coverage-report = callPackage mergedPkgs batsim-coverage-report {} options;
          };
        };
        packages-release = functions.generate-packages (pkgs // base-defs // packages-release) release-options;
        packages-debug = functions.generate-packages (pkgs // base-defs // packages-debug) debug-options;
        packages-debug-cov = functions.generate-packages (pkgs // base-defs // packages-debug-cov) debug-cov-options;
        packages = {
          ci-batsim-werror-gcc = callPackage (pkgs) functions.batsim ({ stdenv = pkgs.gccStdenv; werror = true; } // base-defs) release-options;
          ci-batsim-werror-clang = callPackage (pkgs) functions.batsim ({ stdenv = pkgs.clangStdenv; werror = true; } // base-defs) release-options;
          batsim-release = packages-release.batsim;
          batsim-debug = packages-debug-cov.batsim;
          batsim-internal-test = packages-debug-cov.batsim-internal-test;
          batsim-coverage-report = packages-debug-cov.batsim-coverage-report;
        };
        devShells = {};
      }
    );
}

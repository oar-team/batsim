#
# WARNING: do NOT use old kapack for new work!
# make sure you use NUR-kapack instead!
#
{ old_kapack ? import
  ( fetchTarball "https://github.com/oar-team/kapack/archive/773d3909d78f1043ffb589a725773699210d71d5.tar.gz") {}
, kapack ? import
  ( fetchTarball "https://github.com/oar-team/nur-kapack/archive/1672831224a21d6c34350d8f78cff9266e3e28a2.tar.gz") {}
}:

with kapack.pkgs;

let
  self = rec {
    # an old version of Batsim, before the introduction of smart pointers.
    old_batsim = old_kapack.batsim_dev.overrideAttrs (attr: rec {
      name = "batsim-3.1.0-346e0de";
      src = fetchgit {
        url = "https://framagit.org/batsim/batsim.git";
        rev = "346e0de311c10270d9846d8ea418096afff32305";
        sha256 = "0jacrinzzx6nxm99789xjbip0cn3zfsg874zaazbmicbpllxzh62";
      };
      mesonBuildType = "release";
      hardeningDisable = [];
      dontStrip = false;
    });
    # a more recent Batsim, after the introduction of smart pointers.
    batsim = (kapack.batsim-310.override{simgrid=kapack.simgrid-325;}).overrideAttrs (attr: rec {
      name = "batsim-3.1.0-5906dbe";
      src = fetchgit {
        url = "https://framagit.org/batsim/batsim.git";
        rev = "5906dbe67ba5c6229029e3ddcde5979ae116f287";
        sha256 = "08jwsgiz0s9n15pcv637sq31gyd3qzja850ycaz06kv59jlzcrfb";
      };
      mesonBuildType = "release";
      hardeningDisable = [];
      dontStrip = false;
    });
    # set of scheduling algorithms
    batsched = kapack.batsched-130.overrideAttrs (attr: rec {
      name = "batsched-1.3.0-dev";
      src = fetchgit {
        url = "https://framagit.org/batsim/batsched.git";
        rev = "54b18eb4f24bdb69617baa58b5b07842c70df094";
        sha256 = "08mw03k18m4ppschrcmyali33im4hz8060j990aa673vpdlf0pb2";
      };
    });

    # r tools around Batsim
    battools_r = kapack.pkgs.rPackages.buildRPackage {
      name = "battools-r-fcccf8a";
      src = fetchgit {
        url = "https://framagit.org/batsim/battools.git";
        rev = "fcccf8a6bccae388af6a17b866bba6c11097734f";
        sha256 = "05vll6rhdiyg38in8yl0nc1353fz2j7vqpax64czbzzhwm5d5kfs";
      };
      propagatedBuildInputs = with kapack.pkgs.rPackages; [
        dplyr
        readr
        magrittr
        assertthat
      ];
    };

    # a python tool to transform massif traces to exploitable data
    massif_to_csv = kapack.pkgs.python3Packages.buildPythonPackage rec {
      pname = "massif_to_csv";
      version = "0.1.0";
      propagatedBuildInputs = [msparser];
      src = builtins.fetchurl {
        url = "https://files.pythonhosted.org/packages/09/2d/674c3405939f198e963ba5e73c2a331ef3364bc52da9b123c8f16dd60c8d/massif_to_csv-0.1.0.tar.gz";
        sha256 = "f5eb01dce6d2e4a6c9812fd58f0add20b6739ba340482b7902f311298eb37dfb";
      };
    };

    # a massif_to_csv dependency
    msparser = kapack.pkgs.python3Packages.buildPythonPackage rec {
      pname = "msparser";
      version = "1.4";
      buildInputs = [
        kapack.pkgs.python3Packages.pytest
      ];
      src = builtins.fetchurl {
        url = "https://files.pythonhosted.org/packages/e0/68/aece1c5e75b49d95f304d2df029ae69583ef59a55694ec683e2452d70637/msparser-1.4.tar.gz";
        sha256 = "1199d27bdc492647d2d17d7776e49176f3ec3d2d959d4cfc8b2ce9257cefc16f";
      };
    };

    # dependencies in common for the two experimental environments
    common_expe_deps = [
      kapack.batexpe
      batsched
      valgrind
    ];

    # environment used to generate simulation inputs.
    input_preparation_env = mkShell rec {
      name = "input-preparation-env";
      buildInputs = [
        # to generate robin instances
        kapack.batexpe
        # to generate a batsim workload
        kapack.pkgs.R
        battools_r
        # to download a batsim platform
        curl
      ];
    };
    # environment used to execute simulations with the old Batsim version.
    simulation_old_env = mkShell rec {
      name = "old-env";
      buildInputs = [old_batsim] ++ common_expe_deps;
    };
    # environment used to execute simulations with the recent Batsim version.
    simulation_env = mkShell rec {
      name = "env";
      buildInputs = [batsim] ++ common_expe_deps;
    };
    # environment used to analyze the results, and to render them into a html document.
    notebook_env = mkShell rec {
      name = "notebook-env";
      buildInputs = [
        # Tools to analyse results.
        kapack.pkgs.R
        kapack.pkgs.rPackages.tidyverse
        kapack.pkgs.rPackages.viridis
        massif_to_csv
        # Rmarkdown-related tools (Rmarkdown is a notebook technology).
        kapack.pkgs.rPackages.knitr
        kapack.pkgs.rPackages.rmarkdown
        kapack.pkgs.pandoc
      ];
    };
  };
in
  self

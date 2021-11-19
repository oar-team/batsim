let
  kapack = import
    ( fetchTarball "https://github.com/oar-team/nur-kapack/archive/master.tar.gz") {};
in

let my-packages = rec {
  # define your own batsim version from batsim-4.1.0
  # (you can select another base, such as batsim-310 for batsim-3.1.0)
  my-batsim = kapack.batsim-410.overrideAttrs(old: rec {
    # define where to the source code of your own batsim version.
    # here, use a given commit on the official batsim git repository on framagit,
    # but you can of course use your own commit/fork instead.
    version = "3e7a7e5ecf1b749c306c532f828219e5a3d70862";
    src = kapack.pkgs.fetchgit rec {
      url = "https://framagit.org/batsim/batsim.git";
      rev = version;
      sha256 = "1f3xij13i4wgd667n0sqdkkzn5b11fz4bmxv9s0yivdniwn6md76";
    };
  });

  # define your own version of a scheduler, here from batsched-1.4.0
  my-batsched = kapack.batsched-140.overrideAttrs(old: rec {
    # (you can also your own commit/fork of batsched here)
    version = "b020e48b89a675ae681eb5bcead01035405b571e";
    src = kapack.pkgs.fetchgit rec {
      url = "https://framagit.org/batsim/batsched.git";
      rev = version;
      sha256 = "1dmx2zk25y24z3m92bsfndvvgmdg4wy2iildjmwr3drmw15s75q0";
    };
  });

  # can also be done on the pybatsim scheduler, here from pybatsim-3.2.0
  # (note that overridePythonAttrs is needed instead of overrideAttrs)
  my-pybatsim = kapack.pybatsim-320.overridePythonAttrs(old: rec {
    # (you can also your own commit/fork of pybatsim here)
    version = "fa6600eccd4e5ffbebfc7ecba05b64cf84c5e9d3";
    src = kapack.pkgs.fetchgit rec {
      url = "https://gitlab.inria.fr/batsim/pybatsim.git";
      rev = version;
      sha256 = "0nr8hqsx6y1k0lv82f4x3fjq6pz2fsd3h44cvnm7w21g4v3s6l6y";
    };
  });

  shell = kapack.pkgs.mkShell rec {
    name = "tuto-env";
    buildInputs = [
      my-batsim
      my-batsched
      my-pybatsim
      kapack.batexpe
    ];
  };
};
in
  my-packages.shell

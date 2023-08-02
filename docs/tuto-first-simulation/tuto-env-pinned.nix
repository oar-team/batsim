let
  kapack = import
    ( fetchTarball "https://github.com/oar-team/nur-kapack/archive/master.tar.gz") {};
in

let my-packages = rec {
  # define your own batsim version from batsim-4.2.0
  # (you can select another base, such as batsim-310 for batsim-3.1.0)
  my-batsim = kapack.batsim-420.overrideAttrs(old: rec {
    # define where to the source code of your own batsim version.
    # here, use a given commit on the official batsim git repository on framagit,
    # but you can of course use your own commit/fork instead.
    version = "194c926d4c9c9c83340ef9d5ba17ad47e8b3b67e";
    src = kapack.pkgs.fetchgit rec {
      url = "https://framagit.org/batsim/batsim.git";
      rev = version;
      sha256 = "sha256-RsjvPhuJL6S804/BZGpzl9qkCujWJxWK0isD4osekMI=";
    };
  });

  # define your own version of a scheduler, here from batsched-1.4.0
  my-batsched = kapack.batsched-140.overrideAttrs(old: rec {
    # (you can also your own commit/fork of batsched here)
    version = "b020e48b89a675ae681eb5bcead01035405b571e";
    src = kapack.pkgs.fetchgit rec {
      url = "https://framagit.org/batsim/batsched.git";
      rev = version;
      sha256 = "sha256-AJejS+A1t5F5lY3GKDwnr9W3d7NOL5Hq+ET4IuYXvbY=";
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
      sha256 = "sha256-3lCjxyYvCH6q3YwQOJp24l+DpRudOIE2BTN40zWGKFs=";
    };
  });

  shell = kapack.pkgs.mkShell rec {
    name = "tuto-env";
    buildInputs = [
      my-batsim
      my-batsched
      my-pybatsim
      kapack.batexpe
      kapack.pkgs.curl
    ];
  };
};
in
  my-packages.shell

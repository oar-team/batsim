{ kapack ? import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/773d3909d78f1043ffb589a725773699210d71d5.tar.gz")
  {}
}:

with kapack.pkgs;

let
  self = rec {
    my_batsim = kapack.batsim.overrideAttrs (attr: rec {
      name = "batsim-3.1.0-346e0de";
      src = fetchgit {
        url = "https://framagit.org/batsim/batsim.git";
        rev = "346e0de311c10270d9846d8ea418096afff32305";
        sha256 = "0jacrinzzx6nxm99789xjbip0cn3zfsg874zaazbmicbpllxzh62";
      };
    });
    my_batsched = kapack.batsched.overrideAttrs (attr: rec {
      name = "batsched-1.3.0-db0450a";
      src = fetchgit {
        url = "https://framagit.org/batsim/batsched.git";
        rev = "db0450a608656f0661f4c8d6f132c68b2d402a59";
        sha256 = "05bn43xrk4qz8w2v58zkl17vmj4y57y93lrgrpv7jsdkird9a5vw";
      };
    });
    my_pybatsim = kapack.pybatsim.overrideAttrs (attr: rec {
      name = "pybatsim-3.1.0-ca81c4a";
      src = fetchgit {
        url = "https://gitlab.inria.fr/batsim/pybatsim.git";
        rev = "ca81c4a49b84fb5249367ae64bdc9289d619a033";
        sha256 = "153wxqyz2pgb3skspz9628s91zrsvbzvgxpx6c6sbjharavdnyik";
      };
    });

    experiment_env = mkShell rec {
      name = "experiment_env";
      buildInputs = [
        # simulator
        my_batsim
        # scheduler implementations
        my_batsched
        my_pybatsim
        # misc. tools to execute instances
        kapack.batexpe
      ];
    };
  };
in
  self.experiment_env

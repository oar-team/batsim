{ kapack ? import
    ( fetchTarball "https://github.com/oar-team/kapack/archive/773d3909d78f1043ffb589a725773699210d71d5.tar.gz")
  {}
}:

with kapack.pkgs;

let
  self = rec {
    experiment_env = mkShell rec {
      name = "experiment_env";
      buildInputs = [
        # simulator
        kapack.batsim
        # scheduler implementations
        kapack.batsched
        kapack.pybatsim
        # misc. tools to execute instances
        kapack.batexpe
      ];
    };
  };
in
  self.experiment_env

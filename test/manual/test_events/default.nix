let
  kapack = import (fetchTarball
    #https://github.com/oar-team/kapack/archive/master.tar.gz
    {
      name = "kapack-2019-01-16";
      # Archive from commit hash
      url = https://github.com/oar-team/kapack/archive/2abcea09317df1f85f537b21673650e25c7411df.tar.gz;
      sha256 = "0b9qbsab8mw4jj0kds3if3dmn768xhvg3f03kyvcjb5nhihp87ll";
    }
    ) {};
  inherit (kapack) pkgs;
in
  (pkgs.mkShell {
    buildInputs = let
      batsim = (kapack.batsim_dev.overrideAttrs (oldAttrs: {
       src = fetchGit {
        url = https://gitlab.inria.fr/batsim/batsim.git;
        #rev = "refs/heads/102-add-replay-of-machine-failures";
        ref = "102-add-replay-of-machine-failures";
        };
      }));

      pybatsim = kapack.pybatsim.overrideAttrs (oldAttrs: {
        src = fetchGit {
          url = https://gitlab.inria.fr/batsim/pybatsim.git;
          rev = "b254932d5d22be9054df840125a976980dc7747c";
          ref = "batsim-issue-102";
        };
        propagatedBuiltInputs = oldAttrs.propagatedBuildInputs ++ [ kapack.procset ];
      });

    in [
      batsim
      pybatsim
      kapack.batexpe
    ];
  })

{
  datamove ? import (
    fetchTarball "https://gitlab.inria.fr/vreis/datamove-nix/repository/master/archive.tar.gz") { }
}:
{
  batsim_local_dev = datamove.batsim_dev.overrideAttrs (attrs: rec {
    version = "dev_local";
    src = ./.;
  });
}

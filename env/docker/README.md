This directory contains the definition of the main Docker container used by the CIs of the Batsim ecosystem.

List of files.
- [default.nix](./default.nix) defines how to build the container.
- [Makefile](./Makefile) is a wrapper to build/push the container on
  [Docker Hub](https://hub.docker.com/r/oarteam/batsim_ci/)

List of commands.
- Build: ``make build``
- Push on Docker Hub: ``make push``

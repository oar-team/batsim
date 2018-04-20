This directory contains:
- the [definition](./Dockerfile) of the Docker container used for Batsim
  continuous integration system
- a wrapper to easily update the container and push it on
  [Docker Hub](https://hub.docker.com/r/oarteam/batsim_ci/)

Build
=====

To use docker cache, simply run ``make build``.  
To disable docker cache: ``make build-nocache``.

Push to Docker Hub
==================

``` bash
make push
```

.. _ci:

Continuous Integration
======================

Batsim is tested under Gitlab's continuous integration system.

.. image:: img/ci/overview.svg

- Main CI script is :download:`.gitlab-ci.yml <../.gitlab-ci.yml>` (from Batsim's repository root directory).
- CI logs are available on `Batsim's Framagit Pipelines`_.
- Docker image used for tests is defined in :download:`env/docker/Dockerfile <../env/docker/Dockerfile>` (from Batsim's repository root directory).
- Give a look at CI's script to reproduce locally.
  Enable Batsim's Cachix cache to not recompile dependencies: ``cachix add batsim``.

Additionnally, a Batsim container is deployed on Dockerhub_ for each commit on the master branch.
This is done on Travis_ whose script is defined in :download:`.travis.yml <../.travis.yml>` (from Batsim's repository root directory).

.. _Batsim's Framagit Pipelines: https://framagit.org/batsim/batsim/pipelines
.. _Dockerhub: https://hub.docker.com/repository/docker/oarteam/batsim
.. _Travis: https://travis-ci.org/github/oar-team/batsim

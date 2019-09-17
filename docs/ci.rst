.. _ci:

Continuous Integration
======================

Batsim is tested under Gitlab's continuous integration system.

.. image:: img/ci/overview.svg

- Main CI script is :download:`.gitlab-ci.yml <../.gitlab-ci.yml>` (from Batsim's repository root directory).
- CI logs are available on `Batsim's Framagit Pipelines`_.
- Docker image is defined in :download:`env/docker/Dockerfile <../env/docker/Dockerfile>` (from Batsim's repository root directory).
- Give a look at CI's script to reproduce locally.
  Enable Batsim's Cachix cache to not recompile dependencies: ``cachix add batsim``.

.. _Batsim's Framagit Pipelines: https://framagit.org/batsim/batsim/pipelines

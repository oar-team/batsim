.. _tuto_reproducible_experiment:

Doing a reproducible experiment
===============================

This tutorial shows how to execute a Batsim simulation in a fully reproducible software environment thanks to the Nix_ package manager.
Two schedulers are used here, to show how to use a specific version of batsched_ and pybatsim_.

Prerequisites
-------------

As a small Nix_ knowledge is recommended,
we strongly encourage readers to follow a `tutorial about Nix and reproducible experiments`_ first.
This tutorial introduces main Nix_ concepts and focuses on those useful for reproducible experiments,
lowering the entry cost in the Nix_ ecosystem.

Defining a reproducible environment
-----------------------------------

A first attempt
'''''''''''''''

A direct attempt to define such an environment is to use the packages defined in Kapack_.
This is what the following Nix_ file describes.

.. literalinclude:: ./env-first-attempt.nix
  :caption: :download:`env-first-attempt.nix <./env-first-attempt.nix>`
  :language: nix

Assuming that the :download:`env-first-attempt.nix <./env-first-attempt.nix>` file is in your working directory, you can enter into the environment with the following command: ``nix-shell --pure ./env-first-attempt.nix``.
You should be able to call ``batsim``, ``batsched``, ``pybatsim`` and ``robin`` from inside this shell.
The shell can be exited as usual (Ctrl+D, ``exit``…).

While this shell perfectly works to use the latest release of the desired packages,
it does not describe the desired versions well enough to be reproductible: If you use the same environment in the future, the versions may have changed.

Pinning the package repository
''''''''''''''''''''''''''''''

A first step to improve our setup is to use a specific *pinned* version of Kapack_ (the repository that contains package definitions) instead of its ``master`` branch.

.. literalinclude:: ./env-pin-pkgs-repo.nix
  :caption: :download:`env-pin-pkgs-repo.nix <./env-pin-pkgs-repo.nix>`
  :language: nix

The :download:`env-pin-pkgs-repo.nix <./env-pin-pkgs-repo.nix>` file is exactly the same as the previous one but about its ``kapack`` import definition: `Kapack's commit 773d390`_ is used instead of the ``master`` branch. This makes sure that the versions of the desired packages (``batsim``, ``batsched``…) will not change.

.. warning::

   This is not true if you use the ``-master`` variant of a package!

   For example, ``batsim-master`` represents Batsim built from Batsim's master branch's latest commit – without any kind of commit pinning.

Pinning the packages
''''''''''''''''''''

In many cases you might want to use a specific version of a package rather than its latest release.
Nix_ makes it easy by *overridding* a package definition, as in the following file.

.. literalinclude:: ./env-pin-everything.nix
  :caption: :download:`env-pin-everything.nix <./env-pin-everything.nix>`
  :language: nix

The :download:`env-pin-everything.nix <./env-pin-everything.nix>` file defines custom versions of Batsim, batsched and pybatsim — and uses them in the ``experiment_env`` shell.
This is especially useful for using your own variant of a scheduling algorithm, as you can put the git repository of your choice in the ``url`` field of the ``fetchgit`` command and thus use your own fork_ of a scheduler project.

.. note::

   Filling correctly the ``sha256`` field of the package source overriding can be annoying.

   A fast way to find the right value is to first fill it with a random sha256 value (``echo hello | sha256sum``), then to try to enter your shell.
   Nix_ will whine about the hash mismatch then print the hash value it computed :).
   In the example below, Nix_ computed a sha256 value of ``0jacrinzzx6nxm99789xjbip0cn3zfsg874zaazbmicbpllxzh62``.

   .. code-block:: text

      hash mismatch in fixed-output derivation '/nix/store/vmqrfa7l1hg1lshaj15jz46li5v8r2qs-batsim-346e0de':
        wanted: sha256:00xyyr3fi8l6hb839bv3f7yb86yjv7xi1cgh1xnhipym4asvb4aq
        got:    sha256:0jacrinzzx6nxm99789xjbip0cn3zfsg874zaazbmicbpllxzh62

Setting up a full experiment
----------------------------

.. note::

   A rendering of this experiment notebook is hosted `there <https://mpoquet.github.io/blog/2020-02-batsim-memory-usage-notebook/index.html>`_.

This experiment example shows how Nix can be help in designing an experiment with a reproducible software environment.
Its goal is to evaluate whether the introduction of smart pointers reduced Batsim's memory usage over time or not.

Environments
''''''''''''

.. literalinclude:: ./env-check-memuse-improvement.nix
  :caption: :download:`env-check-memuse-improvement.nix <./env-check-memuse-improvement.nix>`
  :language: nix

The :download:`env-check-memuse-improvement.nix <././env-check-memuse-improvement.nix>` file describes various environments — comments describe the role of each environment.
Several simulation environments are used here as we want to evaluate several versions of the same software (batsim), as using two versions of the same package in the same environment would create a collision.

Scripts
'''''''

Similarly to what would (should) be done in a real experiment, some scripts are used here so that some steps are kept independent from each other and from the main engine used to run the experiment.
Here, a `rmarkdown notebook`_ is used as the main engine to execute the simulation, and to analyze and present the results.

The scripts used here have the following content.

.. literalinclude:: ./generate-workload.R
  :caption: :download:`generate-workload.R <./generate-workload.R>`
  :language: R

.. literalinclude:: ./prepare-instances.bash
  :caption: :download:`prepare-instances.bash <./prepare-instances.bash>`
  :language: bash

.. literalinclude:: ./run-notebook.R
  :caption: :download:`run-notebook.R <./run-notebook.R>`
  :language: R

Notebook
''''''''

Finally, here is the notebook source:

.. literalinclude:: ./notebook.Rmd
  :caption: :download:`notebook.Rmd <./notebook.Rmd>`
  :language: md

The notebook can be run with the following command.

.. code-block:: bash

   nix-shell env-check-memuse-improvement.nix -A notebook_env --command ./run-notebook.R

.. _Nix: https://nixos.org/nix/
.. _tutorial about Nix and reproducible experiments: https://nix-tutorial.gitlabpages.inria.fr/nix-tutorial/
.. _Kapack: https://github.com/oar-team/kapack
.. _Kapack's commit 773d390: https://github.com/oar-team/kapack/commit/773d3909d78f1043ffb589a725773699210d71d5
.. _batsched: https://framagit.org/batsim/batsched/
.. _pybatsim: https://gitlab.inria.fr/batsim/pybatsim
.. _fork: https://en.wikipedia.org/wiki/Fork_(software_development)
.. _rmarkdown notebook: https://rmarkdown.rstudio.com/

.. _tuto_first_simulation:

Running your first simulation
=============================

.. todo::
    Simplify installation in this tutorial (``nix-env -i`` should be enough).
    Write :ref:`installation` and link to it for more possibilities.

    Move this environment presented here to the more advanced tutorial :ref:`tuto_reproducible_experiment`.

This tutorial does not assume you have installed Batsim nor any of its tools.
A Nix_ environment is used instead.
If you already have a working Nix installation, skip `Installing Nix`_ and go
directly to `Getting into the tutorial's environment`_.

Installing Nix
--------------
Installing Nix is pretty straightforward.

.. code:: bash

    curl https://nixos.org/nix/install | sh

Follow the instructions displayed at the end of the script.
You need to source a file to access the Nix commands:

.. code:: bash

    source ~/nix-profiles/etc/profile.d/nix.sh

.. note::

    This is unlikely but the procedure to install Nix_ may be outdated.
    Please refer to `Nix installation documentation`_ for up-to-date installation material.

Getting into the tutorial's environment
---------------------------------------
Nix makes it easy to define environments that bundle several packages together.
Write a file **./tuto-env.nix** with the following content.

.. literalinclude:: ./tuto-env.nix
    :language: nix

This Nix file describes a software environment that makes batsim, batsched and batexpe available to the user.
The exact definition of how these packages are built is defined in the kapack_ repository.

Getting into the defined environment is done thanks to **nix-shell**.
nix-shell first checks that all the requested packages are available in your system.
If this is not the case, nix-shell will first build all required packages.
In this case the required packages are batsim, batsched, batexpe **and all their dependencies**.
To speed the process up, you can use Batsim's binary cache to download our binaries instead of rebuilding them.

.. literalinclude:: ../cachix-setup.bash
    :language: bash
    :lines: 3-

Using nix-shell to get into the environment can be done with ``nix-shell ./tuto-env.nix``.

You should now be in the tutorial's environment if the previous command succeeded!
You can run the following commands to make sure that the different tools are available.

.. literalinclude:: ./show_versions.bash
    :language: bash
    :lines: 3-

The version numbers should be the following.

.. literalinclude:: ./show_versions.out
    :language: text

Leaving the environment can be done by the **exit** bash command, or simply by
pressing Ctrl+D.

Retrieve simulation inputs
--------------------------
Now that the different programs can be used, simulation inputs are needed before executing the simulation.
This tutorial uses a simulated platform and a workload that are both defined in
Batsim's repository.
Retrieving these files is essentially a clone to Batsim's repository at latest release.

.. literalinclude:: ./retrieve_simulation_inputs.bash
    :language: bash
    :lines: 3-

Executing the simulation
------------------------
A Batsim simulation most of the time involves two different processes.

1. Batsim itself, in charge of simulating what happens on the platform
2. A decision process, in charge of taking all the decisions about jobs and resources. This is where scheduling algorithms are implemented.

In this tutorial we will use the batsched_ decision process,
which is the reference implementation of a Batsim decision process.
As running the simulation involves two processes,
launch two terminals that are both into the tutorial's environment — by running ``nix-shell ./tuto-env.nix`` in the two of them.

Batsim will be executed in the first terminal.
First, define an environment variable **EXPE_RESULT_DIR** which contains the directory into which Batsim results will be written —
e.g., with ``export EXPE_RESULT_DIR=/tmp/expe-out``. Please note that this directory must exist.
Make sure the **BATSIM_DIR** environment variable points to the Batsim repo cloned in `Retrieve simulation inputs`_ then run batsim with the following command.

.. literalinclude:: ./run_batsim.bash
    :language: bash
    :lines: 3-

You can now run a scheduler **in the second terminal** to start the simulation. The following command runs batsched with the EASY backfilling algorithm.

.. literalinclude:: ./run_batsched.bash
    :language: bash
    :lines: 3-

The simulation should now start. Both processes should finish soon.
The simulation results should now be written in **EXPE_RESULT_DIR**.

Executing the simulation with robin
-----------------------------------
`Executing the simulation`_ shows how to run one Batsim simulation by executing the two processes manually.
This is important to understand how running a simulation works,
but in most cases executing them by hand is not desired.

This is exactly why robin_ has been created.
Robin allows to easily execute batsim simulations and to see them as
black boxes, which is convenient when simulation campaigns are to be conducted.

One way to use robin is to first define how the simulation should be executed then to execute the simulation.
Generating the **./expe.yaml** file, which describes the simulation instance description executed in `Executing the simulation`_,
can be done thanks to the following command.
Before running it, make sure that the two **BATSIM_DIR** and **EXPE_RESULT_DIR** environment variables are defined in your terminal.

.. literalinclude:: ./run_robin_generate.bash
    :language: bash
    :lines: 3-

Running the simulation can then be done with the following command.

.. literalinclude:: ./run_robin.bash
    :language: bash
    :lines: 3-

Robin's displayed output shows the lifecycle of batsim and the scheduler.
Robin returns 0 if and only if the simulation has been executed successfully.

Robin can directly execute a simulation without creating a description file,
and have different options that makes it easy to call from experiment scripts.
Please refer to ``robin --help`` for more information about these features.

.. _Nix: https://nixos.org/nix/
.. _Nix installation documentation: https://nixos.org/nix/
.. _kapack: https://github.com/oar-team/kapack/
.. _robin: https://framagit.org/batsim/batexpe/
.. _batsched: https://framagit.org/batsim/batsched

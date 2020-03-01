.. _tuto_first_simulation:

Running your first simulation
=============================

Prerequisites
-------------
This tutorial assumes that you have installed Batsim, batsched_ (a set of schedulers for Batsim) and robin_ (an experiment manager for Batsim).

The following Nix_ command installs the three aforementioned packages,
but please read :ref:`Installation` first — as it may save you a lot of compilation time.

.. code:: bash

    nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA batsim batsched batexpe

Checking software environment
-----------------------------
You can run the following commands to make sure that the different tools are installed at the expected version.

.. literalinclude:: ./show_versions.bash
    :language: bash
    :lines: 3-

The version numbers should be the following.

.. literalinclude:: ./show_versions.out
    :language: text

Retrieving simulation inputs
----------------------------
Now that the different programs can be used, simulation inputs are needed to execute the simulation.
This tutorial uses a simulated platform and a workload that are both defined in
Batsim's repository.
Retrieving these files is essentially a clone to Batsim's repository at latest release.

.. literalinclude:: ./retrieve_simulation_inputs.bash
    :language: bash
    :lines: 3-

Preparing the simulation output directory
-----------------------------------------
This is simply the creation of the directory in which the simulation results will be put.

.. literalinclude:: ./prepare_output_directory.bash
    :language: bash
    :lines: 3-

Executing the simulation manually
---------------------------------
A Batsim simulation most of the time involves two different processes.

1. Batsim itself, in charge of simulating what happens on the platform
2. A decision process, in charge of taking all the decisions about jobs and resources. This is where scheduling algorithms are implemented.

As running the simulation involves two processes, first launch two terminals.
Batsim can be executed in the first terminal with the following command.

.. literalinclude:: ./run_batsim.bash
    :language: bash
    :lines: 3-

The scheduler can now be run **in the second terminal** to start the simulation.
In this tutorial we will use the batsched_ decision process,
which is the reference implementation of a Batsim decision process.
The following command runs batsched with the EASY backfilling algorithm.

.. literalinclude:: ./run_batsched.bash
    :language: bash
    :lines: 3-

The simulation should now start. Both processes should finish soon.
The simulation results should now be written in **EXPE_RESULT_DIR**.

Executing the simulation with robin
-----------------------------------
`Executing the simulation manually`_ shows how to run one Batsim simulation by executing the two processes manually.
This is important to understand how running a simulation works,
but in most cases executing them by hand is not desired.

robin_ has been created to make running Batsim simulation easy and robust.
It allows to see Batsim simulations as black boxes,
which is convenient when simulation campaigns are to be conducted.

One way to use robin is to first define how the simulation should be executed then to execute the simulation.
Generating the **./expe.yaml** file, which describes the simulation instance description executed in `Executing the simulation manually`_,
can be done thanks to the following command.

.. literalinclude:: ./run_robin_generate.bash
    :language: bash
    :lines: 3-

Running the simulation can then be done with the following command.

.. literalinclude:: ./run_robin.bash
    :language: bash
    :lines: 3-

Robin's displayed output shows the lifecycle of Batsim and the scheduler.
Robin returns 0 if and only if the simulation has been executed successfully.

Robin can directly execute a simulation without creating a description file,
and have different options that makes it easy to call from experiment scripts.
Please refer to ``robin --help`` for more information about these features.

.. _Nix: https://nixos.org/nix/
.. _Nix installation documentation: https://nixos.org/nix/
.. _kapack: https://github.com/oar-team/nur-kapack/
.. _robin: https://framagit.org/batsim/batexpe/
.. _batsched: https://framagit.org/batsim/batsched

.. _cli:

Command-line Interface
======================

All usages and options of the command-line interface can be shown by executing ``batsim --help``.
We present some examples that shows how you can use and combine the different options of Batsim's CLI.
All examples suppose that you are located in the root folder of Batsim.

Note that we do not give examples of the External Decision Component (EDC) command, in the case you are using an EDC as a process, as it depends on the :ref:`EDC implementation<tuto_sched_implem>` you use.


First run
---------

A single process simulation with a small platform and only one computation job workload as input, using a FCFS dynamic library, is done with the command:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json \
    -l /path/to/fcfs.so ''

When using EDC libraries, you can specify to use `dlmopen` as the load method instead of the default `dlopen` with the option `edc-library-load-method dlmopen`.


The same simulation, using an external decision component as a process with its initialisation file, is done with the command:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json \
    -S 'tcp://localhost:28000' /path/to/EDC_init_file


Configuration file
------------------

All Command-line options may be read from a configuration file in TOML or INI format.

To generate a configuration file containing all command-line options of a batsim call add the following option: `--gen-config config_file.toml`

Then you can simply run your simulation with the command:

.. code:: bash

    batsim -c config_file.toml




Using external events
---------------------

If you want to include :ref:`input_EVENTS` in your simulation with an input file containing external events that you defined you can run:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json \
        -l /path/to/fcfs.so '' \
        --ee events/my_generic_events.txt



Example with various options
----------------------------

Finally, if you want to simulate

    - a long workload and a worklaod for energy testing at the same time,
    - on the ``energy_platform.xml``,
    - making host ``Mars`` a ``storage`` resource,
    - with energy support
    - an extra simgrid logging option to set the level of the energy pluggin as critical,
    - specifying a folder in the prefix of the output files,
    - with a process EDC and a specific initialisation string,
    - and batsim logging on debug level,

you can run:

.. code:: bash

    batsim -w workloads/test_long_workload.json \
        -w workloads/test_energy_minimal_load100.json \
        -p platforms/energy_platform.xml \
        --add-role-to-hosts Mars:storage \
        -E --sg-log surf_energy.thresh:critical \
        --export simu_outputs/ \
        -s "ipc://foobar" '{"init_option1": "foo", "init_option2": 23}' \
        -v debug

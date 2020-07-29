.. _cli:

Command-line Interface
======================

All usages and options of the command-line interface can be shown by executing ``batsim --help``.
We present some examples that shows how you can use and combine the different options of Batsim's CLI.
All examples suppose that you are locate in the root folder of Batim.

Note that we do not give examples of the scheduler command, as it depends on the
:ref:`scheduling implementation<tuto_sched_implem>` you use.


First run
---------

A simple simulation with a small platform and only one computation job workload as input is done on the Batsim side with the command:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json



Using dynamic jobs
------------------

If you want to enable :ref:`dynamic_job_registration` and profiles by your scheduler during the simulation
and you want Batsim to acknowledge registered jobs, you can run:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json \
        --enable-dynamic-jobs --acknowledge-dynamic-jobs



Using Redis
-----------

If you want to use :ref:`Redis<redis>` during your simulation and specify its hostname, you can run:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json \
        --enable-redis --redis-hostname 123.246.0.4



Using external events
---------------------

If you want to include :ref:`input_EVENTS` in your simulation with an input file containing external events know by Batsim
and an input file containing generic events that you defined you can run:

.. code:: bash

    batsim -p platforms/small_platform.xml -w workloads/test_one_computation_job.json \
        --events events/test_events_4hosts.txt \
        --events events/my_generic_events.txt --forward-unknown-events



Example with various options
----------------------------

Finally, if you want to simulate

    - a long workload and a worklaod for energy testing at the same time,
    - on the ``energy_platform.xml``,
    - making host ``Mars`` a ``storage`` resource,
    - enabling sharing of the compute resources,
    - with redis enabled on the specified port ``6789``,
    - with energy support
    - an extra simgrid logging option to set the level of the energy pluggin as critical,
    - specifying a folder in the prefix of the output files,
    - specifying the socket endpoint to communicate with the scheduler,
    - batsim logging on debug level,

you can run:

.. code:: bash

    batsim -p platforms/energy_platform.xml -w workloads/test_long_workload.json \
        -w workloads/test_energy_minimal_load100.json \
        --add-role-to-hosts Mars:storage \
        --enable-compute-sharing \
        --enable-redis --redis-port 6789 \
        -E --sg-log surf_energy.thresh:critical \
        --export simu_outputs/out \
        --socket-endpoint ipc://foobar \
        -v debug

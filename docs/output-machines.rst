.. _output_machines:

Agregated machine state trace
=============================

Machine state change tracing can be enabled with the option `--trace-machine-state` (see :ref:`cli`).

This file is written as Batsim's export *prefix* + ``machine_states.csv`` (the prefix is `out/` by default, see :ref:`cli`).
This is a time series that contains the number of hosts in each kind of state over time.
Please refer to :ref:`platform_energy_model` for more information about the existing kinds of states.
It contains the following fields in this order.

- ``time``: The time at which the measure has been done.
- ``nb_sleeping``: The number of hosts currently in a sleeping power state.
- ``nb_switching_on``: The number of hosts currently transitioning into a computation power state.
- ``nb_switching_off``: The number of hosts currently transitioning into a sleeping power state.
- ``nb_idle``: The number of hosts currently in a computation power state, but without a job running on them.
- ``nb_computing``: The number of hosts currently in a computation power state, with a job running on them.

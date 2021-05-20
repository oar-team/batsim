.. _output_energy:

Energy
======

When a Batsim simulation is run with energy enabled (see :ref:`cli`),
Batsim outputs energy-specific output files.

You can give a look at `this evalys visualization example <https://github.com/oar-team/evalys/tree/master/examples/poquetm>`_ to see how to use such data.

Energy consumption trace
------------------------

This file is written as Batsim's export prefix + ``_consumed_energy.csv``.
This is a time series that contains the energy consumption of the platform
(as defined as the sum of all the computing hosts of the platform) from time 0 to the current time.
It contains the following fields in this order.

- ``time``: The time point at which the measure has been done.
- ``energy``: The amount of energy consumed in joules by the platform from time 0 to ``time``. |br|
  **This metrics is robust, you can analyze/visualize it without concerns.**
- ``wattmin``: The minimum current power consumption of the platform,
  taking into account the power state into which each host is,
  and assuming that all hosts have an *epsilon* load (close to 0 without reaching it).
- ``epower``: The average power consumption of the whole platform since last event to the current one.
  **Use this value with caution** as it can be subject to degenerate cases — *e.g.*, when two successive events happen at the same ``time``.
- ``event_type``: The type of the event that occurred at time ``time``.

  - ``s`` if the event is a job start
  - ``e`` if the event is a job end
  - ``p`` if the event is a host power state change


Power state change trace
------------------------

This file is written as Batsim's export prefix + ``_pstate_changes.csv``.
This is a time series that contains the power state transitions of the hosts over time.
It contains the following fields in this order.

- ``time``: The time at which the power state transition occurred.
- ``machine_id``: The :ref:`interval_set` of hosts whose power state has been changed.
- ``new_pstate``: The new power state (an integer) of the involved hosts.

Agregated machine state trace
-----------------------------

This file is written as Batsim's export prefix + ``_machine_states.csv``.
This is a time series that contains the number of hosts in each kind of power state over time.
Please refer to :ref:`platform_energy_model` for more information about the existing kinds of power states.
It contains the following fields in this order.

- ``time``: The time at which the measure has been done.
- ``nb_sleeping``: The number of hosts currently in a sleeping power state.
- ``nb_switching_on``: The number of hosts currently transitioning into a computation power state.
- ``nb_switching_off``: The number of hosts currently transitioning into a sleeping power state.
- ``nb_idle``: The number of hosts currently in a computation power state, but without a job running on them.
- ``nb_computing``: The number of hosts currently in a computation power state, with a job running on them.

.. |br| raw:: html

   <br />

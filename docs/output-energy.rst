.. _output_energy:

Energy consumption trace
========================

When a Batsim simulation is run with energy enabled (see :ref:`cli`), Batsim outputs an energy-specific output file. |br|
You can give a look at `this evalys visualization example <https://github.com/oar-team/evalys/tree/master/examples/poquetm>`_ to see how to use such data.

This file is written as Batsim's export *prefix* + ``consumed_energy.csv`` (the prefix is `out/` by default, see :ref:`cli`).
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


.. |br| raw:: html

   <br />

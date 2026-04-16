.. _output_pstate:

Power state change trace
========================

Power state change tracing can be enabled with the option `--trace-pstate-change` (see :ref:`cli`).

This file is written as Batsim's export *prefix* + `pstate_changes.csv`(the prefix is `out/` by default, see :ref:`cli`).
This is a time series that contains the power state transitions of the hosts over time.
It contains the following fields in this order.

- ``time``: The time at which the power state transition occurred.
- ``machine_id``: The :ref:`interval_set` of hosts whose power state has been changed.
- ``new_pstate``: The new power state (an integer) of the involved hosts.

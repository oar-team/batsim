.. _input_EVENTS:

External Events
=============

0verview and example
--------------------

This is a mechanism to enable injection of exernal events during the simulation.
For example, one would be interested in studying the behavior and resilience of a scheduling policy when machine failures occurr over time,
which is made possible using the events injection mechanism!


Supported Events
----------------

Here is the list of supported external events that can be injected during the simulation:

- Machine_failure_
- Machine_restore_

Each external event is notified to the scheduler via a :ref:`proto_NOTIFY` protocol event.

Machine_failure
~~~~~~~~~~~~~~~

Simulates a failure of the specified machines and kills every job that was running on these machines.
In addition to the :ref:`proto_NOTIFY` protocol event, a :ref:`proto_JOB_COMPLETED` protocol event with a `COMPLETED_RESOURCE_FAILED` state is sent to the scheduler for each killed job.

Machine_restore
~~~~~~~~~~~~~~~

Simulates a restoration of the previously failed machines, putting them in ``IDLE`` state.

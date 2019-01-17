.. _input_EVENTS:

External Events
===============

0verview and example
--------------------

This is a mechanism to enable injection of exernal events during the simulation.
For example, one would be interested in studying the behavior and resilience of a scheduling policy when machine failures occur over time,
which is made possible using the events injection mechanism!


Input File Format
-----------------

An example of an input file for external events can be found in ``events/test_events_4hosts.txt``.
Such file should contain on each line a JSON description of an event, as follows:

.. code:: json

    {"type": "machine_failure", "resources": "0-2", "timestamp" : 0}
    {"type": "machine_restore", "resources": "0", "timestamp" : 10}
    {"type": "machine_restore", "resources": "1-2", "timestamp" : 20.6}

Note that it is not mandatory to have the events ordered by their timestamps in the input files, as they will be sorted anyway once all events of a file are loaded by Batsim.
In each event description, the field ``resources`` contains the list of resources on which the event occurs (represented as an :ref:`interval_set`) and the field ``timestamp`` contains the date at which the event has to occur during the simulation.

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


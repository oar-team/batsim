.. _input_EVENTS:

External Events
===============

0verview and example
--------------------

This is a mechanism to enable injection of exernal events during the simulation.
For example, one would be interested in studying the behavior and resilience of a scheduling policy when machine can become unavailable for a certain period of time,
which is made possible using the events injection mechanism!


Input File Format
-----------------

An example of an input file for external events can be found in ``events/test_events_4hosts.txt``.
Such file should contain on each line a JSON description of an event, as in

.. literalinclude:: ../events/test_events_4hosts.txt

Note that it is not mandatory to have the events ordered by their timestamps in the input files, as they will be sorted anyway once all events of a file are loaded by Batsim.
In each event description, the field ``resources`` contains the list of resources on which the event occurs (represented as an :ref:`interval_set`) and the field ``timestamp`` contains the date at which the event has to occur during the simulation.

Supported Events
----------------

Here is the list of supported external events that can be injected during the simulation:

- Machine_unavailable_
- Machine_available_

Each external event is notified to the scheduler via a :ref:`proto_NOTIFY` protocol event.

Machine_unavailable
~~~~~~~~~~~~~~~~~~~

The specified machines become unavailable. It is no longer possible to execute jobs on these machines, but jobs that were already running on these machines are not killed.

Machine_available
~~~~~~~~~~~~~~~~~

The specified machines become available. It is now possible to execute jobs on these machines.

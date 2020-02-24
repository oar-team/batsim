.. _input_EVENTS:

External Events
===============

Overview and example
--------------------

This is a mechanism to enable injection of external events during the simulation.
For example, one would be interested in studying the behavior and resilience of a scheduling policy when machine can become unavailable for a certain period of time,
which is made possible using the events injection mechanism!


Input File Format
-----------------

An example of an input file for external events can be found in ``events/test_events_4hosts.txt``.
Such file should contain on each line a JSON description of an event, as in

.. literalinclude:: ../events/test_events_4hosts.txt

Note that it is not mandatory to have the events ordered by their timestamps in the input files, as they will be sorted anyway once all events of a file are loaded by Batsim.
In each event description, the field ``type`` contains the type of the external event that occurs and the field ``timestamp`` contains the date at which the event has to occur during the simulation.
Other fields may be present, depending on the event type, as described in `Supported Events`_.

Supported Events
----------------

Here is the list of supported external events that can be injected during the simulation:

- Machine_unavailable_
- Machine_available_

Each external event is notified to the scheduler via a :ref:`proto_NOTIFY` protocol event.

Machine_unavailable
~~~~~~~~~~~~~~~~~~~

The machines specified by the `resources` field (represented as an :ref:`interval_set`) become unavailable.
It is no longer possible to execute jobs on these machines, but **jobs that were already running on these machines are not killed**.

.. code:: json

    {"type": "machine_unavailable", "resources": "0-4 6 8", "timestamp" : 10}

Machine_available
~~~~~~~~~~~~~~~~~

The machines specified by the `resources` field (represented as an :ref:`interval_set`) become available.
It is now possible to execute jobs on these machines **if no job is running on them** (unless resource sharing is enabled).

.. code:: json

    {"type": "machine_available", "resources": "2-4 8", "timestamp" : 20}

.. _events_GENERIC_EVENTS:

Generic_events
~~~~~~~~~~~~~~

External events unknown to Batsim can also be added in the input file.
These events are not supported by Batsim unless the option ``--forward-unknown-event`` is set. In this case, such events will be forwarded to the scheduler without any check (except for the `type` and `timestamp` fields).


.. code:: json

    {"type": "user_specific_type", "timestamp": 150, "some_field": "value_of_field", "another_field": 36}

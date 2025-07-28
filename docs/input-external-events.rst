.. _input_EVENTS:

External Events
===============

This is a mechanism to enable injection of external events during the simulation.
For example, one would be interested in studying the behavior and resilience of a scheduling policy when machines can become unavailable for a certain period of time,
which is made possible using the events injection mechanism!


Each external event is notified to the External Decision Component (EDC) via a ``ExternalEventOccurredEvent`` protocol event (see :ref:`protocol` for more details).

.. note::

    The information about each external event are simply forwarded to the EDC.
    Batsim will not change the course of the simulation upon the occurrence of an external event.
    It is the EDC's responsibility to react and make decisions about the simulation when an external event occurs.


Input File Format
-----------------

An example of an input file for external events can be found in ``events/test_generic_events.txt``.
Such file should contain on each line a JSON description of an event, as in

.. literalinclude:: ../events/test_generic_events.txt

Note that it is not mandatory to have the events ordered by their timestamps in the input files, as they will be sorted anyway once all events of a file are loaded by Batsim.

In each event description, the field ``type`` contains the type of the external event that occurs and the field ``timestamp`` contains the date at which the event has to occur during the simulation.
Other fields may be present, depending on the event type, as described in `Supported External Events`_.

Supported External Events
-------------------------

Batsim currently supports only ``generic`` event types.
In addition to the ``timestamp`` and ``type`` filed, a generic event contains a ``data`` field which must contain a string.
This allows for a variety of use-cases by using string-formatted JSON objects, for example:

The following is an example of using external events to tell the EDC that some machines became unavailable.
The ``data`` string contains a stringified JSON object with two fields: ``type`` which tells some machines became unavailable, and ``resources`` (represented as an :ref:`interval_set`) which tells which machines are impacted.

.. code:: json

    {"type": "generic", "timestamp":  10.0, "data": "{\"type\": \"machine_unavailable\", \"resources\": \"0-4 6 8\"}"}

.. note::
    
    Do not forget to escape the double quotes (``\"``) when using a JSON objects in the ``data`` field.

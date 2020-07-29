.. _redis:

Using Redis
===========

By default, Batsim's :ref:`protocol` transfers job metadata directly on the socket between Batsim and the *Decision Process*.
Alternatively, you can use a `Redis server <https://redis.io/>`_ to transfer this information.
Redis support is enabled by providing the ``--enable-redis`` :ref:`cli` flag to the Batsim command.

.. warning::

    Redis should also be configured in your :ref:`Scheduler implementation <tuto_sched_implem>`.
    Redis information is forwarded to the scheduler in the :ref:`SIMULATION_BEGINS protocol event <proto_SIMULATION_BEGINS>`.

Key prefix
----------

Since several Batsim instances can be run at the same time,
all the keys explained in this document must be prefixed by some instance-specific prefix.
The key prefix can be set by the ``--redis-prefix`` :ref:`cli` option.
Batsim will write and read keys by prefixing them by the user-given prefix followed by a colon ``:``.

List of used keys
-----------------

Job information
"""""""""""""""
As soon as job ``<ID>`` is submitted within Batsim, the ``job_<ID>`` key is set.
Its value is the JSON_ description of the job (cf. :ref:`job_definition`).

.. note::

    When using Redis with :ref:`dynamic_job_registration`, Batsim expects to find job information in the Redis store when it receives a :ref:`proto_REGISTER_JOB` event.

Profile information
"""""""""""""""""""
If profile forwarding is enabled (``--forward-profiles-on-submission`` :ref:`cli` option, forwarded in :ref:`proto_SIMULATION_BEGINS`),
the profile of jobs is also set in the Redis store at job submission time.
If the submitted job uses the ``<PROF>`` profile, the ``profile_<PROF>`` key is set.
Its value is the JSON_ description of the profile (cf. :ref:`profile_definition`).

.. note::

    When using Redis with :ref:`dynamic_job_registration`, Batsim expects to find profile information in the Redis store when it receives a :ref:`proto_REGISTER_PROFILE` event.

.. _JSON: https://www.json.org

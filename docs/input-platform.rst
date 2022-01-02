.. _input_platform:

Platform
========
A Batsim platform is essentially a SimGrid platform.
This page describes the specificities of Batsim platforms in comparison with SimGrid platforms. More information on how to define the SimGrid platform of your dreams can be found on the `SimGrid documentation`_ ;).

.. _platform_host_roles:

Roles of hosts
--------------

A host in Batsim can have one among several roles:

- ``master``
- ``compute_node``
- ``storage``


In Batsim, the management of the simulation and the platform is given to a special host having the ``master`` role.
There must and can be only one master host.
The master host is a dummy host (it is not known by the scheduler) that is executing the server process and every other processes to manage the simulation,
such as the request-reply process that talks to the scheduler, processes to submit launch and kill jobs, etc.

By default, every host of the platform, except for the ``master host``, has the ``compute_node`` role, meaning that
jobs performing computations can be executed on these hosts.

The last role a host can have is ``storage``.
A ``storage`` host should have a ``speed`` value of ``0f``.
Such host cannot perform any computation but can be used to execute jobs with IO profiles
(more details about IO profiles can be found in the :ref:`input_workload` documentation).


It is possible to specify the role of a host in the platform description by adding a property to that host.
For example:

.. code:: xml

    <host id="storage_server" speed="0f">
        <prop id="role" value="storage"/>
    </host>

Roles can also be added via the :ref:`cli` command ``-r, --add-role-to-hosts <hosts_role_map>``, where
``hosts_role_map`` is formated as ``<hostname1[,hostname2,...]>:<role>``.

.. _platform_energy_model:

Energy model
------------
SimGrid allows each host to have a set of power states.
Each state defines several properties.

- The computation speed (number of floating-point operations per second).
- The electrical energy consumption (in watts). This is usually defined with several values.

Switching from one power state to another is instantaneous in SimGrid — the idea is to allow simulators to implement whatever they want with the simple proposed model.

Batsim adds a simple layer on top of the SimGrid one so that switching from a power state to another may take a fixed amount of time and energy. This has been developed with node shutdown techniques in mind.

First, Batsim proposes to split the set of power states in three.

- Computation power states. These model energy and performance trade-offs of the computation machine — e.g., dynamic voltage and frequency scaling. **Only these can be used to compute jobs**.
- Sleep power states. These model how much energy is used during the sleeping state itself — e.g., node shutdown or suspend to RAM.
- Transition power states. These *virtual* power state model how long and how much energy takes the transition from one state to another.

Batsim users (= schedulers) can only switch into computation or sleep power states. Switching from one sleep power state to another is forbidden. Switching from one computation power state to another is instantaneous.

.. todo::

    - Finish to describe the Batsim energy model.
    - Add instantiation examples.

.. _SimGrid documentation: https://simgrid.org/doc/latest/Platform.html

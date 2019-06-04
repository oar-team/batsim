.. _input_platform:

Platform
========
A Batsim platform is essentially a SimGrid platform.
This page describes the specificities of Batsim platforms in comparison with SimGrid platforms. More information on how to define the SimGrid platform of your dreams can be found on the SimGrid documentation ;).

.. _platform_host_roles:

Types of hosts
--------------

.. todo::

    Talk about the types of hosts (computation, storage, master_host)...

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


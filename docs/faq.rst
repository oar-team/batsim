.. _faq:

Frequenty Asked Questions (FAQ)
===============================

Simulation / SimGrid
--------------------

This section lists questions related to simulation or to SimGrid.

What is a parallel task?
~~~~~~~~~~~~~~~~~~~~~~~~

Parallel tasks refer to two different SimGrid things.

- First, this is a high-level view of a set of computations and communications.
  As I write these lines, SimGrid proposes to use them via its parallel execution API
  ``simgrid::s4u::this_actor::parallel_execute``
  (see `SimGrid's documentation on executions on the CPU`_).
- Second, this is the SimGrid model ``ptask_L07``,
  which is needed to use the SimGrid parallel execution API (the first item of this list).
  This model defines how computations are done on the CPU,
  but contrary to most other SimGrid CPU models
  ``ptask_LO7`` also handles the progress of network activities at the same time.

A parallel task is a bundle of computations and network transfers
that occur on a given number of hosts :math:`N`.
A parallel task is thus defined as:

- A computation amount (in number of floating-point operations) for each host
  (there are :math:`N` values).
  Each value represent the amount of work executed by the host.
- A communication amount (in bytes) for each **directed** pair of host
  (there are :math:`N^2` values).
  Each value represent the amount of data sent from each host to another.

.. note::

    A given parallel task is not bound to particular hosts,
    this is just a computation vector and a communication matrix.

    An **ordered** list of hosts is given when you execute a parallel task,
    and hosts in the list do **not** need to be **unique**.
    This means you can instantiate a parallel task of size 4 in various ways:

    - You can use hosts {1, 3, 5, 7}.
      The first computation value will be used on host 1,
      the second on host 3, the third on host 5, the fourth on host 7...
    - You can use hosts {3, 1, 5, 7}.
      This is the same as before except that computation values will be placed
      differently: the first computation value will be used on host 3 and the
      second computation value will be used on host 1.
      **This change also affects network transfers** in a similar fashion.
    - You can use hosts {1, 1, 1, 1}.
      In this case all computation will be done on the host 1,
      and all network transfers will be done on host 1 loopback link.

    Related questions:

    - :ref:`faq_job_inner_placement`
    - :ref:`faq_per_core_placement`


How is the execution of a parallel task simulated?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contrary to a set of independent executions or network transfers,
the sub-elements of a parallel tasks are **totally dependent**.

All sub-elements of a parallel task progress in a **completely synchronous** fashion.
In other words, the advance rate of each individual computation and network transfer
is **exactly** the same at each (simulation world) time.

Please note that this does not prevent parallel tasks progress to adapt to their
execution context.
The advance rate always remains the same for all sub-elements of a parallel tasks,
but **this advance rate may have different values over (simulation world) time**.
SimGrid computes what the current bottleneck is
(the lowest advance rate among all sub-elements) at each simulation step,
and apply this advance rate to all sub-elements of the parallel task.

For example, if a parallel task is executed alone
(without any other computation or network transfer done on the simulated platform),
it would progress at advance rate :math:`\alpha_{alone}`.
If the same parallel task is executed while some of its resources
(SimGrid hosts or links) are shared with other SimGrid activities
(computations, network transfers, other parallel tasks...),
it would progress at advance rate :math:`\alpha \le \alpha_{alone}`.
Please note that the non-strictness of the inferior operator (:math:`\le`) is important here.
If two parallel tasks share the same resources but
the first parallel task's bottleneck is defined by executions,
while the other parallel tasks's bottleneck is defined by network transfers,
it is possible that the parallel task do not slow each other if they are run
concurrently.

.. _faq_cpu_core:

What is a CPU core in SimGrid?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A lie :).

Contrary to real-world CPU cores, SimGrid cores do not exist as distinct entities.
In SimGrid, this is just properties of a host
(hosts do exist as distinct entities in SimGrid).
In particular, hosts have a per-core computation speed :math:`{core}_{speed}`
(denoted as ``speed`` in SimGrid XML platforms as I write these lines)
and a number of cores :math:`nb_{cores}`
(denoted as ``core`` in SimGrid XML platforms as I write these lines).
These properties are used to limit the computation speed of activities on a host.

- The speed of each computation :math:`c` is bound by the core computation speed:
  :math:`speed_c \le {core}_{speed}`.
- The sum of the speed of computations that occur on a host is bound by the
  core computation speed multiplied by the number of cores:
  :math:`\sum_c speed_c \le {core}_{speed} \times nb_{cores}`.

In short, this enables computations that occur on the same host to impact each other
a lot when there are more computations than cores,
while this impact does not exist if there are up to :math:`nb_{cores}` computations
(or is small depending on the CPU sharing model used).

Consequently, **Batsim does NOT enable schedulers to reserve or allocate cores**
as they do not exist in SimGrid.

See also: :ref:`faq_per_core_placement`.

.. _faq_job_inner_placement:

Can the scheduler decide the (inner) placement of a job?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes. When you execute a job you essentially give the following information:

- A set of hosts ``host_allocation`` where the job shoud be executed.
  Please note that these allocations are currently given as
  the canonical representation of an :ref:`interval_set`, which **must be ordered**.
- A mapping that defines where each SimGrid executor should take place
  among ``host_allocation``.
  This mapping is optional if you want one SimGrid executor per host
  (executor 0 will be put on host 0 of ``host_allocation``,
  executor 1 will be put on host 1 of ``host_allocation``...).

The number of SimGrid executors is defined by your simulation profile.
Typically for parallel tasks,
the number of SimGrid executors is the size of the computation vector.

If the simulation profile of your job is not homogeneous,
the job inner placement (the mappingf is important as it may impact the job
execution time.

.. _faq_per_core_placement:

How to execute jobs that have per-core profiles?
------------------------------------------------

Currently, you have to use a custom execution mapping to do so
(see :ref:`faq_job_inner_placement`).

For example if your profile is a parallel task of size 4 where each executor
represents a core, you can decide to execute it on a single host
(let us say host 5) by giving ``host_allocation`` = ``5``
and a custom mapping that places all executors on the first allocated host
``mapping`` = ``{"0": "0", "1": "0", "2": "0", "3": "0"}``.
**This is possible regardless of the number of cores on host 5**,
the number of cores on host 5 and their speed will only impact the job execution
time (see :ref:`faq_cpu_core`).

You can decide another placement for that same parallel task of size 4.
For example, you can decide to use two hosts and place the first two executors
on the first allocated host (host 7),
and the other two on the second allocated host (host 13).
The following values describe this scenario.

- ``host_allocation`` = ``7 13``
- ``mapping`` = ``{"0": "0", "1": "0", "2": "1", "3": "1"}``

Or to use the same two hosts, but with another placement strategy.
Here, executors 0 and 2 will be on the first allocated host,
while the others are on the second allocated host.
The following values describe this scenario.

- ``host_allocation`` = ``7 13``
- ``mapping`` = ``{"0": "0", "1": "1", "2": "0", "3": "1"}``

.. _SimGrid's documentation on executions on the CPU: https://simgrid.org/doc/latest/app_s4u.html#executions-on-the-cpu

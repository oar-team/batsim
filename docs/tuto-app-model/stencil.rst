.. _tuto_app_modelling_stencil:

Modelling a stencil application in Batsim
==========================================

This tutorials shows how a parallel application with simple patterns can be modelled in a Batsim workload.
Here, we will assume that the target application is a simulation using `Iterative Stencil Loops`_.

First, keep in mind that **there is no right way to model an application in Batsim**.
How to model an application mostly depends on what phenomena you want to observe and at which granularity. Here is the context used in this tutorial.

- Stay coarse grain.
  We plan to use this application inside workloads to study scheduling algorithms,
  we do not plan to gain insight on the application itself.
- We want the application to be sensitive to its execution context: If any used resource is under congestion while the the application runs, its execution time should be longer.
- We want the application to impact how CPUs, network links, switches and disks are loaded over time. In other words, we want the application to potentially degrade the performance of other applcations and itself.

Details on the imaginary application
------------------------------------

The imaginary application studied here follows an `Iterative Stencil Loops`_ pattern.
The application takes a two-dimensional matrix of 32-bit floating-point values as input, of fixed size :math:`4096 \times 4096` elements.
It computes 1000 *iterations* on the matrix.
Each iteration consists in updating each value in the matrix according to the values of its neighbors.
The application is parallel and always uses 4 processes.
Spatial parallelism is used here: Each process works on a fourth of the total matrix.
The sub-matrices held in memory by each process are not totally disjoint: Each process has a tiny border (1 row or column) of data coming from neighboring processes, which is exchanged between neighboring processes for each iteration.

.. image:: stencil-ranks.svg
  :width: 300
  :alt: Stencil ranks

Temporally speaking, the big steps of the applications are the following.

1. Spawn the 4 parallel processes. In real life, this would be done by mpirun or similar.
2. Each parallel process reads input data from a parallel file system.
   Here, all processes read the same amount of data. The total matrix size is :math:`4096 \times 4096 \times 4 = 67108864` bytes, and each process should read :math:`2048 \times 2048 \times 4 = 16777216` bytes.
3. All processes do a series of 100 iterations. Each iteration consists in the following.

  1. Each process updates the values of the sub-matrix it is responsible for.
     Let us say this computation takes :math:`10` floating-point operations for each value, which amounts for around :math:`10^7` floating-point operations for the whole sub-matrix.
  2. Each process shares the border of its sub-matrix to its direct neighbors.
     Here, as we only have 4 processes, each process communicates with 2 other processes as shown on the figure above. The data transfered from one process to its neighbor is a row or column of the sub-matrix — *i.e.*, :math:`2048 \times 4 = 8192` bytes.

4. Every 100 iterations, a checkpoint of the current data is done.
   This is a common fault tolerance practice, so that if a machine fails the computation can be restarted from *quite recent* data, not the initial one.
   This checkpoint consists in writing the whole matrix on a parallel file system.
   The total amount of data transfered is therefore :math:`4096 \times 4096 \times 4 = 67108864` bytes, in other words :math:`2048 \times 2048 \times 4 = 16777216` bytes per process.
5. Once all 1000 iterations are done the application stops (that last checkpoints is the final application output).

Job modelling
-------------

A Batsim :ref:`input_workload` consists of jobs and profiles.

- Jobs define the view of the application from the scheduler: This is a user request.
- Profiles define the view of the application from Batsim: This is how to simulate the application.

Here is a job description of such an application.

.. code-block:: json

   {"id":"0", "subtime":0, "walltime": 3600, "res": 4, "profile": "imaginary_stencil"}

- As said previously, this application is always run on ``4`` processes.
- We arbitrarily chose the job to be identified by ``0`` and to have a walltime of ``3600`` seconds —
  If it takes longer than this to be executed, Batsim will kill it automatically.
- Finally, we say that the job will be simulated using the ``imaginary_stencil`` profile, which is detailed in next section.

Profile modelling
-----------------

.. _Iterative Stencil Loops: https://en.wikipedia.org/wiki/Iterative_Stencil_Loops

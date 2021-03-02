.. _tuto_app_modelling_stencil:

Modelling a stencil application in Batsim
==========================================

This tutorial shows how a parallel application with simple patterns can be modelled in a Batsim workload.
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
The sub-matrices held in memory by each process are not totally disjoint: Each process has a tiny border (1 row or column) of data coming from neighboring processes, which is exchanged between neighboring processes at each iteration.

.. image:: stencil-ranks.svg
  :width: 300
  :alt: Stencil ranks

Temporally speaking, the big steps of the applications are the following.

1. Spawn the 4 parallel processes. In real life, this would be done by ``mpirun`` or similar.
2. Each parallel process reads input data from a parallel file system.
   Here, all processes read the same amount of data. The total matrix size is :math:`4096 \times 4096 \times 4 = 67108864` bytes, and each process should read :math:`2048 \times 2048 \times 4 = 16777216` bytes.
3. All processes do a series of 100 iterations. Each iteration consists of the following.

  1. Each process updates the values of the sub-matrix it is responsible for.
     Let us say this computation takes :math:`10` floating-point operations for each value, which amounts for around :math:`10^7` floating-point operations for the whole sub-matrix.
  2. Each process shares the borders of its sub-matrix to its direct neighbors.
     Here, as we only have 4 processes, each process communicates with 2 other processes as shown on the figure above. The data transferred from one process to its neighbors is a row or column of the sub-matrix — *i.e.*, :math:`2048 \times 4 = 8192` bytes.

4. Every 100 iterations, a checkpoint of the current data is done.
   This is a common fault tolerance practice, so that if a machine fails the computation can be restarted from *quite recent* data, not the initial one.
   This checkpoint consists in writing the whole matrix on a parallel file system.
   The total amount of data transferred is therefore :math:`4096 \times 4096 \times 4 = 67108864` bytes, with :math:`2048 \times 2048 \times 4 = 16777216` bytes per process.
5. Once all 1000 iterations are done the application stops (the last checkpoint is the final application output).

Job modelling
-------------

A Batsim :ref:`input_workload` consists of jobs and profiles.

- Jobs define the view of the application from the scheduler: This is a user request.
- Profiles define the view of the application from Batsim: This is how to simulate the application.

Here is a job description of such an application:

.. code-block:: json

   {"id":"0", "subtime":0, "walltime": 3600, "res": 4, "profile": "imaginary_stencil"}

- As said previously, this application is always run on ``4`` processes (the ``res`` key).
- We arbitrarily chose the job to be identified by ``0`` and to have a walltime of ``3600`` seconds —
  If it takes longer than this to be executed, Batsim will kill it automatically.
- Finally, we say that the job will be simulated using the ``imaginary_stencil`` profile, which is detailed in the next section.

Profile modelling
-----------------

Let us model the various subparts of the application.

Data transfers with the parallel file system
############################################

First, the initial load of the matrix can be modelled with a :ref:`profile_parallel_homogeneous_pfs` profile like this, which will read ``67108864`` bytes from a storage host called ``pfs`` and homogeneously split the data towards all the hosts involved in the job (here, ``4`` hosts).

.. code-block:: json

    {
      "type": "parallel_homogeneous_pfs",
      "bytes_to_read": 67108864,
      "bytes_to_write": 0,
      "storage": "pfs"
    }

The checkpoints of the data produced by the application are very similar, the operation is just a ``write`` instead of a ``read``.

.. code-block:: json

    {
      "type": "parallel_homogeneous_pfs",
      "bytes_to_read": 0,
      "bytes_to_write": 67108864,
      "storage": "pfs"
    }


Iteration
#########

The communication part of each iteration can be modelled with a :ref:`profile_parallel` profile,
which corresponds to the data transferred between each pair of hosts in the job (here, ``4`` hosts). The data here corresponds to the transfer of 1 row/column between neighboring processes, as seen on the application details figure.

.. code-block:: json

    {
      "type": "parallel",
      "cpu": [   0,    0,    0,    0],
      "com": [   0, 8192, 8192,    0,
              8192,    0,    0, 8192,
              8192,    0,    0, 8192,
                 0, 8192, 8192,    0]
    }

The computation part of each iteration can also be modelled with a :ref:`profile_parallel` profile such as the following.

.. code-block:: json

    {
      "type": "parallel",
      "cpu": [ 1e7,  1e7,  1e7,  1e7],
      "com": [   0,    0,    0,    0,
                 0,    0,    0,    0,
                 0,    0,    0,    0,
                 0,    0,    0,    0]
    }

Putting all pieces together
###########################

Finally, we can see how to put the previous sub-profiles together to define the ``imaginary_stencil`` profile.
There are many ways to do so. **In a real-life scenario on a real application, you should evaluate the prediction precision of the different choices** to decide which model fits your application (probably with a simulation overhead trade-off in mind). Here is a set of profiles that define ``imaginary_stencil``, bundling a sequence of 100 iterations together in the same :ref:`profile_parallel`.

.. code-block:: json

    {
      "initial_load": {
        "type": "parallel_homogeneous_pfs",
        "bytes_to_read": 67108864,
        "bytes_to_write": 0,
        "storage": "pfs"
      },
      "100_iterations": {
        "type": "parallel",
        "cpu": [   1e9,    1e9,    1e9,    1e9],
        "com": [     0, 819200, 819200,      0,
                819200,      0,      0, 819200,
                819200,      0,      0, 819200,
                     0, 819200, 819200,      0]
      },
      "checkpoint": {
        "type": "parallel_homogeneous_pfs",
        "bytes_to_read": 0,
        "bytes_to_write": 67108864,
        "storage": "pfs"
      },
      "iterations_and_checkpoints": {
        "type": "composed",
        "repeat": 10,
        "seq": ["100_iterations", "checkpoint"]
      },
      "imaginary_stencil": {
        "type": "composed",
        "repeat": 1,
        "seq": ["initial_load", "iterations_and_checkpoints"]
      }
    }

- The ``initial_load`` and ``checkpoint`` profiles are exactly the same as in the `Data transfers with the parallel file system`_ section.
- The ``100_iterations`` :ref:`profile_parallel` profile is the merge of 100 consecutive iterations into a single :ref:`profile_parallel`.
- The ``iterations_and_checkpoints`` :ref:`profile_sequence` represents most of the application, as it repeats ``10`` times the execution of (``100_iterations`` followed by a ``checkpoint``).
- Finally, the ``imaginary_stencil`` profile represents the whole application, which is ``initial_load`` followed by ``iterations_and_checkpoints``.

Impact of this modelling choice
###############################

Here, we chose to group series of 100 iterations together into a single :ref:`profile_parallel`.
This means that during the whole ``100_iterations`` :ref:`profile_parallel`, the resource consumption induced by the profile is quite homogeneous: Communications and computations will be done at the same time, at the rate of the slowest of the two depending on the execution context.
This means bursty phenomena induced by each iteration will not be observable and that the latency of communication phases will only be paid once, not once per iteration.
However, as the ``checkpoint`` is run alone, the burst on the parallel file system induced by the checkpoints will be observable, as well as the latency to communicate with the parallel file system for each checkpoint.

.. _Iterative Stencil Loops: https://en.wikipedia.org/wiki/Iterative_Stencil_Loops

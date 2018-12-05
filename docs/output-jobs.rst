.. _output_jobs:

Jobs
====

The main Batsim output file is exported as *prefix* + ``_jobs.csv`` (see :ref:`cli`).

This file is formatted as CSV_ (with a header) and give information about jobs.
There is one line per job.
The following columns are defined in the file.

- ``job_id``, the job identifier. Value is unique within a workload.
- ``workload_name``, the name of the workload the job belongs to.
- ``submission_time``, the (simulation world) time (in seconds) at which the job has been submitted.
- ``requested_number_of_resources``.
- ``requested_time``, the walltime of the job.
- ``success``, whether the job has completed before reaching its walltime and returned 0.
- ``starting_time``, the (simulation world) time (in seconds) at which the job execution has been started.
- ``execution_time``, the (simulation world) duration (in seconds) of the job execution. Equals to :math:`finish\_time - starting\_time`.
- ``finish_time``, the (simulation world) time (in seconds) at which the job execution has finished.
- ``turnaround_time``, the time the job spend in the system. Equals to :math:`finish\_time - submission\_time`.
- ``stretch``: equals to :math:`turnaround\_time / execution\_time`.
- ``allocated_resources``, a description of the resources onto which the job has been allocated.
- ``consumed_energy``, The total amount of energy (in joules) consumed by the machines during the execution of the job.
- ``metadata``, user-specified metadata about the job (empty string by default).

.. todo::

    Document interval set format. Link here and in protocol.

    Formatted as in the ``from_string_hyphen`` method examples in the `intervalset C++ library quick example`_.

.. _CSV: https://en.wikipedia.org/wiki/Comma-separated_values
.. _intervalset C++ library quick example: https://intervalset.readthedocs.io/en/latest/usage.html#quick-example

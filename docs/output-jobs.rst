.. _output_jobs:

Jobs
====

The main Batsim output file is exported as *prefix* + ``_jobs.csv`` (see :ref:`cli`).

This file is formatted as CSV_ (with a header) and give information about jobs.
There is one line per job.
This file has the following fields in this order.

- ``job_id``, the job identifier. Value is unique within a workload.
- ``workload_name``, the name of the workload the job belongs to.
- ``profile``, the name of the profiles that defines how the job should be simulated.
- ``submission_time``, the (simulation world) time (in seconds) at which the job has been submitted.
- ``requested_number_of_resources``, the number of resources requested by the job.
- ``requested_time``, the requested time of the job (walltime).
- ``success``, whether the job has completed before reaching its walltime and returned 0.
- ``final_state``, the job final state. Possible values include the following.

  - ``COMPLETED_SUCCESSFULLY``: Job executed without reaching walltime, with zero exit code.
  - ``COMPLETED_FAILED``: Job executed without reaching walltime, with non-zero exit code.
  - ``COMPLETED_WALLTIME_REACHED``: Job executed but reached its walltime (killed automatically).
  - ``COMPLETED_KILLED``: Job executed but killed by the decision process.
  - ``REJECTED``: Job not executed, it was rejected by the decision process.
- ``starting_time``, the (simulation world) time (in seconds) at which the job execution has been started.
- ``execution_time``, the (simulation world) duration (in seconds) of the job execution. Equals to :math:`finish\_time - starting\_time`.
- ``finish_time``, the (simulation world) time (in seconds) at which the job execution has finished.
- ``waiting_time``, the (simulation world) time (in seconds) the job waited before being executed.
  Equals to :math:`starting\_time - submission\_time`.
- ``turnaround_time``, the time the job spend in the system. Equals to :math:`finish\_time - submission\_time`.
- ``stretch``: equals to :math:`turnaround\_time / execution\_time`.
- ``consumed_energy``, the total amount of energy (in joules) consumed by the ``allocated_resources`` during the execution of the job. Warning: no energy sharing or allocation is done in case there is more than one job running on a machine.
- ``allocated_resources``, the resources onto which the job has been allocated (see :ref:`interval_set_string_representation`).
- ``metadata``, user-specified metadata about the job (empty string by default).

Please note that many fields can have empty values for jobs that have been rejected.

.. _CSV: https://en.wikipedia.org/wiki/Comma-separated_values

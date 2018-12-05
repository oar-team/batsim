.. _output_schedule:

Schedule
========

Aggregated information about the schedule is exported as *prefix* + ``_schedule.csv`` (see :ref:`cli`).
The file is formatted as CSV_. It only contains a header and a single entry for the whole simulation.
The following columns are defined in the file.

- ``batsim_version``: Similar to the output of the ``--version`` :ref:`cli` option.
- ``consumed_joules``: The total amount of joules consumed by the machines from the submission time of the first job to the finish time of the last job.
- ``makespan``: The completion time of the last job.
- ``max_slowdown``
- ``max_turnaround_time``
- ``max_waiting_time``
- ``mean_slowdown``
- ``mean_turnaround_time``
- ``mean_waiting_time``
- ``nb_computing_machines``
- ``nb_grouped_switches``
- ``nb_jobs``
- ``nb_jobs_finished``
- ``nb_jobs_killed``
- ``nb_jobs_success``
- ``nb_machine_switches``
- ``scheduling_time``: The (real world) time (in seconds) spent in the scheduler (and in the network).
- ``simulation_time``: The (real world) duration (in seconds) of the whole simulation.
- ``success_rate``: :math:`nb\_jobs\_success / nb\_jobs`
- ``time_computing``: Total time of all machines spent in computing state.
- ``time_idle``: Total time of all machines spent in idle state.
- ``time_sleeping``: Total time of all machines spent in sleeping state.
- ``time_switching_off``: Total time of all machines spent in switching_off state.
- ``time_switching_on``: Total time of all machines spent in switching_on state.

.. todo::
    Document all fields.
    Document machine states and link to it.

.. _CSV: https://en.wikipedia.org/wiki/Comma-separated_values

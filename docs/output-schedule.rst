.. _output_schedule:

Schedule
========

Aggregated information about the schedule is exported as *prefix* + ``_schedule.csv`` (see :ref:`cli`).
The file is formatted as CSV_. It only contains a header and a single entry for the whole simulation.
This file contains the following fields in lexicographic order of the fields name.

- ``batsim_version``: Similar to the output of the ``--version`` :ref:`cli` option.
- ``consumed_joules``: The total amount of joules consumed by the machines from the submission time of the first job to the finish time of the last job.
- ``makespan``: The time that elapses from the submission time of the first job to the finish time of the last job. It is calculated by `max(finish_time) - min(submission_time)`.
- ``max_slowdown``: The maximum slowdown observed on a job.
  Slowdown is computed for a job as its turnaround time divided by its execution time.
- ``max_turnaround_time``: The maximum turnaround time observed on a job.
  Turnaround time is computed for a job as its finish time minus its submission time.
- ``max_waiting_time``: The maximum waiting time observed on a job.
  Waiting time is computed for a job as its starting time minus its submission time.
- ``mean_slowdown``: The average slowdown observed on jobs.
  Slowdown is computed for a job as its turnaround time divided by its execution time.
- ``mean_turnaround_time``: The average turnaround time observed on jobs.
  Turnaround time is computed for a job as its finish time minus its submission time.
- ``mean_waiting_time``: The average waiting time observed on jobs.
  Waiting time is computed for a job as its starting time minus its submission time.
- ``nb_computing_machines``: The number of computing machines in the simulation.
- ``nb_grouped_switches``: The number of host power state transitions requested by the decision process.
- ``nb_jobs``: The number of jobs in the simulation.
- ``nb_jobs_finished``: The number of finished jobs in the simulation.
- ``nb_jobs_killed``: The number of killed jobs in the simulation.
- ``nb_jobs_success``: The number of jobs that finished successfully in the simulation.
- ``nb_machine_switches``: The number of host power state transitions done on machines.
  This can be seen as a *flattened* version of ``nb_grouped_switches`` over machines.
- ``scheduling_time``: The (real world) time (in seconds) spent in the scheduler (and in the network).
- ``simulation_time``: The (real world) duration (in seconds) of the whole simulation.
- ``success_rate``: :math:`nb\_jobs\_success / nb\_jobs`
- ``time_computing``: Total time of all machines spent in computing state.
- ``time_idle``: Total time of all machines spent in idle state.
- ``time_sleeping``: Total time of all machines spent in sleeping state.
- ``time_switching_off``: Total time of all machines spent in switching_off state.
- ``time_switching_on``: Total time of all machines spent in switching_on state.

.. _CSV: https://en.wikipedia.org/wiki/Comma-separated_values

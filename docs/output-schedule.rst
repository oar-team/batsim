.. _output_schedule:

Schedule
========

Information on the schedule can be found in the `out_schedule.csv` file.
This file has only one entry for the whole simulation, with the following fields:

- `batsim_version`: The output of the `--version` command line argument of Batsim's executable
- `consumed_joules`: The total amount of Joules consumed by the platform
- `makespan`
- `max_slowdown`
- `max_turnaround_time`
- `max_waiting_time`
- `mean_slowdown`
- `mean_turnaround_time`
- `mean_waiting_time`
- `nb_computing_machines`
- `nb_grouped_switches`
- `nb_jobs`
- `nb_jobs_finished`
- `nb_jobs_killed`
- `nb_jobs_success`
- `nb_machine_switches`
- `scheduling_time`: Seconds used by the scheduler
- `simulation_time`: Seconds used for the whole simulation
- `success_rate`: `nb_jobs_success / nb_jobs`
- `time_computing`: Total time of all machines spent in computing state
- `time_idle`: Total time of all machines spent in idle state
- `time_sleeping`: Total time of all machines spent in sleeping state
- `time_switching_off`: Total time of all machines spent in switching_off state
- `time_switching_on`: Total time of all machines spent in switching_on state

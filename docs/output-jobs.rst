.. _output_jobs:

Jobs
====

The main output file of Batsim is `out_jobs.csv`.
After a header with all column names this file writes, with one line per job, the following information about that job:

- `job_id`
- `workload_name`
- `submission_time
- `requested_number_of_resources`
- `requested_time`: The walltime of the job
- `success`: Whether the job has completed successfully
- `starting_time`
- `execution_time`
- `finish_time`
- `turnaround_time`: The time the job spend in the system. Equal to `finish_time - submission_time`
- `stretch`: Equal to `turnaround_time / runtime`
- `allocated_resources`
- `consumed_energy`: The total amount of energy (in Joules) consumed by the machines during the execution of the job
- `metadata`

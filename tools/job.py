#!/usr/bin/env python3

"""Convenient class to store Job information."""

from sortedcontainers import SortedSet


class Job:
    """Convenient class to store Job information."""

    def __init__(self,
                 job_id,
                 nb_res,
                 wait_time,
                 run_time,
                 submit_time,
                 wall_time):
        """Job constructor."""
        self.job_id = job_id
        self.nb_res = nb_res
        self.wait_time = wait_time
        self.run_time = run_time
        self.submit_time = submit_time
        self.wall_time = wall_time

        self.start_time = self.submit_time + self.wait_time
        self.finish_time = self.start_time + self.run_time
        self.turnaround_time = self.finish_time - self.submit_time
        self.stretch = self.turnaround_time / self.nb_res

        self.resources = SortedSet()

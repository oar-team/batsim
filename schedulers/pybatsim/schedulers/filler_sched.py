#/usr/bin/python3

from batsim.batsim import BatsimScheduler, Batsim

import sys
import os
from random import sample
from sortedcontainers import SortedSet


class FillerSched(BatsimScheduler):

    def __init__(self):
        pass

    def onAfterBatsimInit(self):
        self.nb_completed_jobs = 0

        self.jobs_res = {}
        self.jobs_completed = []
        self.jobs_waiting = []

        self.sched_delay = 5.0

        self.openJobs = set()
        self.availableResources = SortedSet(range(self.bs.nb_res))
        self.previousAllocations = dict()


    def scheduleJobs(self):
        scheduledJobs = []

        print('openJobs = ', self.openJobs)
        print('available = ', self.availableResources)
        print('previous = ', self.previousAllocations)

        # Iterating over a copy to be able to remove jobs from openJobs at traversal
        for job in set(self.openJobs):
            nb_res_req = job.requested_resources

            if nb_res_req <= len(self.availableResources):
                res = self.availableResources[:nb_res_req]
                self.jobs_res[job.id] = res
                self.previousAllocations[job.id] = res
                scheduledJobs.append(job)

                for r in res:
                    self.availableResources.remove(r)

                self.openJobs.remove(job)

        # update time
        self.bs.consume_time(self.sched_delay)

        # send to uds
        if len(scheduledJobs) > 0:
            self.bs.start_jobs(scheduledJobs, self.jobs_res)

        print('openJobs = ', self.openJobs)
        print('available = ', self.availableResources)
        print('previous = ', self.previousAllocations)
        print('')

    def onJobSubmission(self, job):
        self.openJobs.add(job)
        self.scheduleJobs()

    def onJobCompletion(self, job):
        for res in self.previousAllocations[job.id]:
            self.availableResources.add(res)
        self.previousAllocations.pop(job.id)
        self.scheduleJobs()

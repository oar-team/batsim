#/usr/bin/python3

from batsim.batsim import BatsimScheduler
from schedulers.common_pyss_adaptator import CpuSnapshot

import sys
import os
from random import sample
from sortedcontainers import SortedSet




class EasyBackfillNotopo(BatsimScheduler):
    """
    An EASY backfill scheduler that doesn't care about topology.
    Adapted from the Pyss simulator.
    """

    def __init__(self):
        pass

    def onAfterBatsimInit(self):
        self.cpu_snapshot = CpuSnapshot(self.bs.nb_res, False)
        self.unscheduled_jobs = []

        self.jobs_res = {}

        self.sched_delay = 5.0

        self.availableResources = SortedSet(range(self.bs.nb_res))
        self.previousAllocations = dict()

    def onJobSubmission(self, just_submitted_job):
        """ Here we first add the new job to the waiting list. We then try to schedule
        the jobs in the waiting list, returning a collection of new termination events """
        # TODO: a probable performance bottleneck because we reschedule all the
        # jobs. Knowing that only one new job is added allows more efficient
        # scheduling here.
        current_time = self.bs.time()
        self.cpu_snapshot.archive_old_slices(current_time)
        self.unscheduled_jobs.append(just_submitted_job)
        
        retl = []
        
        if (self.cpu_snapshot.free_processors_available_at(current_time) >= just_submitted_job.requested_resources):
            self._schedule_jobs(current_time)

    def onJobCompletion(self, job):
        for res in self.previousAllocations[job.id]:
            self.availableResources.add(res)
        self.previousAllocations.pop(job.id)
        current_time = self.bs.time()
        """ Here we first delete the tail of the just terminated job (in case it's
        done before user estimation time). We then try to schedule the jobs in the waiting list,
        returning a collection of new termination events """
        self.cpu_snapshot.archive_old_slices(current_time)
        self.cpu_snapshot.delTailofJobFromCpuSlices(job)
        
        self._schedule_jobs(current_time)



    def _schedule_jobs(self, current_time):
        "Schedules jobs that can run right now, and returns them"
        scheduledJobs = self._schedule_head_of_list(current_time)
        scheduledJobs += self._backfill_jobs(current_time)
        if len(scheduledJobs) > 0:
            
            
            for job in scheduledJobs:
                job.start_time = current_time#just to be sure
                res = self.availableResources[:job.requested_resources]
                self.jobs_res[job.id] = res
                self.previousAllocations[job.id] = res
                for r in res:
                    self.availableResources.remove(r)
            
            self.bs.start_jobs(scheduledJobs, self.jobs_res)
        

    def _schedule_head_of_list(self, current_time):
        tosched = []
        while True:
            if len(self.unscheduled_jobs) == 0:
                break
            # Try to schedule the first job
            if self.cpu_snapshot.free_processors_available_at(current_time) >= self.unscheduled_jobs[0].requested_resources:
                job = self.unscheduled_jobs.pop(0)
                job.start_time = current_time
                self.cpu_snapshot.assignJob(job, current_time)
                tosched.append(job)

                
                
            else:
                # first job can't be scheduled
                break
        return tosched




    def _backfill_jobs(self, current_time):
        """
        Find jobs that can be backfilled and update the cpu snapshot.
        """
        if len(self.unscheduled_jobs) <= 1:
            return []
        
        result = []
        first_job = self.unscheduled_jobs[0]
        shadow_time = self.cpu_snapshot.jobEarliestAssignment(first_job, current_time)
        shadow_len = shadow_time - current_time
        #this can be done at the same time as jobEarliestAssignment
        extra_cpu = self.cpu_snapshot.free_processors_available_at(shadow_time)-first_job.requested_resources
        nonextra_cpu = self.cpu_snapshot.free_processors_available_at(current_time)-extra_cpu
        if nonextra_cpu < 0:
            extra_cpu += nonextra_cpu#<=> =self.cpu_snapshot.free_processors_available_at(current_time)
            nonextra_cpu = 0

        for job in self.unscheduled_jobs[1:]:
            if job.requested_time > shadow_len:
                if job.requested_resources <= extra_cpu:
                    result.append(job)
                    job.start_time = current_time
                    self.cpu_snapshot.assignJob(job, current_time)
                    extra_cpu -= job.requested_resources
            else:
                if job.requested_resources <= extra_cpu+nonextra_cpu:
                    result.append(job)
                    job.start_time = current_time
                    self.cpu_snapshot.assignJob(job, current_time)
                    nonextra_cpu -= job.requested_resources
                    if nonextra_cpu < 0:
                        extra_cpu += nonextra_cpu
                        nonextra_cpu = 0

        for job in result:
            self.unscheduled_jobs.remove(job)

        return result

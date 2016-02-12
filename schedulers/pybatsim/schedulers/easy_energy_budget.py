#/usr/bin/python3

from easy_backfill import *


class EasyEnergyBudget(EasyBackfill):
    def __init__(self, options):
        self.options = options
        
        self.budget_total = 200*1*20+100*6*20
        self.budget_left = self.budget_total
        self.budget_start = 10
        self.budget_end = 30
        
        self.power_idle = 100
        self.power_compute = 200
        
        self.powercap = self.budget_total/(self.budget_end-self.budget_start)
        
        self.already_asked_to_awaken_at_end_of_budget = False
        
        super(EasyEnergyBudget, self).__init__(options)


    def onNOP(self):
        current_time = self.bs.time()
        self._schedule_jobs(current_time)


    def estimate_if_job_fit_in_energy(self, job, start_time, listFreeSpace=None):
        """
        compute the power consumption of the cluster
        if <job> is not Non,e then add the power consumption of this job as if it were running on the cluster.
        """
        
        #if the job does not cross the budget interval
        if not(start_time < self.budget_end and self.budget_start <= start_time+job.requested_time):
            return True
        
        if listFreeSpace is None:
            listFreeSpace = self.listFreeSpace
        
        free_procs = listFreeSpace.free_processors - job.requested_resources
        
        used_procs = self.bs.nb_res - free_procs
        
        power = used_procs*self.power_compute + free_procs*self.power_idle
        
        return power <= self.powercap

    def allocJobFCFS(self, job, current_time):
        #overrinding parent method
        
        if not self.estimate_if_job_fit_in_energy(job, current_time):
            return None
        
        return super(EasyEnergyBudget, self).allocJobFCFS(job, current_time)

    def allocJobBackfill(self, job, current_time):
        #overrinding parent method
        
        if not self.estimate_if_job_fit_in_energy(job, current_time):
            return None
        
        return super(EasyEnergyBudget, self).allocJobBackfill(job, current_time)

    def findAllocFuture(self, job):
        #overrinding parent method

        listFreeSpaceTemp = copy.deepcopy(self.listFreeSpace)
        
        if len(self.listRunningJob) == 0:
            #this condition happen if the powercap is so low that no job can be started now (perhaps some jobs will be backfilled, but we do not it yet)
            #we will ask batsim to wake up us at the end of the budget interval, just to be sure.
            if (self.bs.time() < self.budget_end) and not self.already_asked_to_awaken_at_end_of_budget:
                self.already_asked_to_awaken_at_end_of_budget = True
                self.bs.wake_me_up_at(self.budget_end)
            alloc = listFreeSpaceTemp.assignJob(listFreeSpaceTemp.firstItem, job, self.budget_end)
            #we find a valid allocation
            return (alloc, self.budget_end)

        previous_current_time = self.bs.time()
        for j in self.listRunningJob:
            new_free_space_created_by_this_unallocation = listFreeSpaceTemp.unassignJob(j)

            fit_in_procs = job.requested_resources <= new_free_space_created_by_this_unallocation.res
            
            fit_in_energy = self.estimate_if_job_fit_in_energy(job, j.finish_time, listFreeSpaceTemp)

            if fit_in_procs and fit_in_energy:
                alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, j.finish_time)
                #we find a valid allocation
                return (alloc, j.finish_time)

            if fit_in_procs and not fit_in_energy and self.budget_end < j.finish_time:
                curtime = max(previous_current_time, self.budget_end)
                alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, curtime)
                #we find a valid allocation
                return (alloc, curtime)
            
            previous_current_time = j.finish_time
        
        #We can be here only if a the budget end later that the last running job and that the current job does not fit in energy
        alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, self.budget_end)
        #we find a valid allocation
        return (alloc, self.budget_end)
        
        
        
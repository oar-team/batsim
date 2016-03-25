#/usr/bin/python3

from easyBackfill import *
from intervalContainer import *


class EasyEnergyBudget(EasyBackfill):
    def __init__(self, options):
        self.options = options
        
        self.budget_total = options["budget_total"]
        self.budget_start = options["budget_start"]#the budget limitation start at this time included
        self.budget_end = options["budget_end"]#the budget end just before this time
        
        self.allow_FCFS_jobs_to_use_budget_saved_measured = options["allow_FCFS_jobs_to_use_budget_saved_measured"]
        
        self.reduce_powercap_to_save_energy = options["reduce_powercap_to_save_energy"]
        self.estimate_energy_jobs_to_save_energy = not self.reduce_powercap_to_save_energy
        
        
        self.budget_saved_measured = 0.0
        self.budget_reserved = 0.0
        self.listJobReservedEnergy = []
        
        self.power_idle = options["power_idle"]
        self.power_compute = options["power_compute"]
        
        self.powercap = self.budget_total/(self.budget_end-self.budget_start)
        
        self.already_asked_to_awaken_at_end_of_budget = False
        
        self.monitoring_regitered = False
        self.monitoring_period = options["monitoring_period"]
        self.monitoring_last_value = None
        self.monitoring_last_value_time = None
        
        self.opportunist_shutdown = options["opportunist_shutdown"]
        self.pstate_switchon = options["pstate_switchon"]
        self.pstate_switchoff = options["pstate_switchoff"]
        if self.opportunist_shutdown:
            #Node lifetime:
            # listFreeSpace => "Computing" => listFreeSpace
            # listFreeSpace => self.shuttingdown_nodes and self.shutteddown_nodes => self.shutteddown_nodes => listFreeSpace
            self.shuttingdown_nodes = IntervalContainer()
            self.shutteddown_nodes = IntervalContainer()
            self.to_switchon_on_switchoff = []
            #when we want t ostart a job that use swithedoff nodes
            #we first wake up nodes and then start the job
            # the waiting jobs are put there:
            self.waiting_allocs = []
            

        if self.reduce_powercap_to_save_energy == self.estimate_energy_jobs_to_save_energy:
            assert False, "can't activate reduce_powercap_to_save_energy and estimate_energy_jobs_to_save_energy"


        super(EasyEnergyBudget, self).__init__(options)



    def onJobCompletion(self, job):
        try:
            self.listJobReservedEnergy.remove(job)
            if not hasattr(job, "last_power_monitoring"):
                job.last_power_monitoring = job.start_time
            self.budget_reserved -= job.reserved_power*(job.requested_time -(job.last_power_monitoring - job.start_time))
            assert self.budget_reserved >= 0
        except ValueError:
            pass
        
        super(EasyEnergyBudget, self).onJobCompletion(job)



    def regiter_next_monitoring_event(self):
        next_event = self.bs.time()+self.monitoring_period
        if self.bs.time() >= self.budget_end:
            return
        if next_event > self.budget_end:
            next_event = self.budget_end
        self.bs.wake_me_up_at(next_event)



    def onJobSubmission(self, just_submitted_job):
        if not self.monitoring_regitered:
            self.monitoring_regitered = True
            if self.budget_start != self.bs.time():
                self.bs.wake_me_up_at(self.budget_start)
            else:
                self.onNOP()

        super(EasyEnergyBudget, self).onJobSubmission(just_submitted_job)



    def onReportEnergyConsumed(self, consumed_energy):
        
        if self.monitoring_last_value is None:
            self.monitoring_last_value = consumed_energy
            self.monitoring_last_value_time = self.bs.time()
        
        consumed_energy = consumed_energy - self.monitoring_last_value
        
        maximum_energy_consamble = (self.bs.time()-self.monitoring_last_value_time)*self.powercap
        
        self.budget_saved_measured += maximum_energy_consamble - consumed_energy
        
        
        self.monitoring_last_value = consumed_energy
        self.monitoring_last_value_time = self.bs.time()
        
        for job in self.listJobReservedEnergy:
            assert job in self.listRunningJob
            if not hasattr(job, "last_power_monitoring"):
                job.last_power_monitoring = job.start_time
            self.budget_reserved -= job.reserved_power*(self.bs.time()-job.last_power_monitoring)
            assert self.budget_reserved >= 0
            job.last_power_monitoring = self.bs.time()
        
        self._schedule_jobs(self.bs.time())
        
        self.regiter_next_monitoring_event()



    def onNOP(self):
        current_time = self.bs.time()
        
        if current_time >= self.budget_start:
            self.bs.request_consumed_energy()
        
        if current_time >= self.budget_end:
            self._schedule_jobs(current_time)

    def onMachinePStateChanged(self, nodeid, pstate):
        if pstate == self.pstate_switchoff:
            self.shuttingdown_nodes.removeInterval(nodeid, nodeid)
            #self.shutteddown_nodes.addInterval(nodeid, nodeid) #does nothing
            if nodeid in self.to_switchon_on_switchoff:
                self.to_switchon_on_switchoff.remove(nodeid)
                pstates_to_change = {}
                pstates_to_change[nodeid] = self.pstate_switchon
                self.bs.change_pstates(pstates_to_change)
            return
        elif pstate == self.pstate_switchon:
            self.shutteddown_nodes.removeInterval(nodeid, nodeid)
            allocs_to_start = []
            for (j, (s,e)) in self.waiting_allocs:
                inter = self.shutteddown_nodes.intersection(s,e)
                if len(inter) == 0:
                    #the job is already assign in the freespace structure
                    allocs_to_start.append((j, (s,e)))
                    break #only one node have its pstate changed at a time
            for a in allocs_to_start:
                self.waiting_allocs.remove(a)
            self.bs.start_jobs_continuous(allocs_to_start)


    def _schedule_jobs(self, current_time):
        allocs = self.allocHeadOfList(current_time)
        
        if len(self.listWaitingJob) > 1:
            first_job = self.listWaitingJob.pop(0)
            allocs += self.allocBackFill(first_job, current_time)
            self.listWaitingJob.insert(0, first_job)
        
        
        #switchoff idle resources
        if self.opportunist_shutdown and (current_time < self.budget_end and self.budget_start <= current_time):
            pstates_to_change = {}
            for l in self.listFreeSpace.generator():
                toshuts = self.shutteddown_nodes.difference(l.first_res, l.last_res)
                for toshut in toshuts:
                    self.shutteddown_nodes.addInterval(l.first_res, l.last_res)
                    self.shuttingdown_nodes.addInterval(toshut[0], toshut[1])
                    for i in range(toshut[0], toshut[1]+1):
                        pstates_to_change[i] = self.pstate_switchoff
            self.bs.change_pstates(pstates_to_change)
        if len(allocs) > 0:
            if self.opportunist_shutdown:
                #if an alloc used shutted down nodes, first switch on them
                pstates_to_change = {}
                allocs_to_remove = []
                for (j, (s,e)) in allocs:
                    intersects = self.shutteddown_nodes.intersection(s,e)
                    if len(intersects) != 0:
                        for (istart, iend) in intersects:
                            #nodes that are currently switching to off will be switched to on later
                            rinter = self.shuttingdown_nodes.intersection(istart, iend)
                            for (rrs, rre) in rinter:
                                for i in range(rrs, rre+1):
                                    self.to_switchon_on_switchoff.append(i)
                            rdiffs = self.shuttingdown_nodes.difference(istart, iend)
                            for rdiff in rdiffs:
                                (ristart, riend) = rdiff
                                for i in range(ristart, riend+1):
                                    pstates_to_change[i] = self.pstate_switchon
                        #remove alloc
                        allocs_to_remove.append((j, (s,e)))
                        self.waiting_allocs.append((j, (s,e)))
                for a in allocs_to_remove:
                    allocs.remove(a)
            
                self.bs.change_pstates(pstates_to_change)
        
        
        self.bs.start_jobs_continuous(allocs)
        

    def estimate_energy_running_jobs(self, current_time):
        e = 0
        for j in self.listRunningJob:
            rt = min(j.estimate_finish_time, self.budget_end)
            e += j.requested_resources*self.power_compute*(rt-current_time)
        return e

    def estimate_if_job_fit_in_energy(self, job, start_time, listFreeSpace=None, canUseBudgetLeft=False):
        """
        compute the power consumption of the cluster
        if <job> is not None then add the power consumption of this job as if it were running on the cluster.
        """
        
        #if the job does not cross the budget interval
        if not(start_time < self.budget_end and self.budget_start <= start_time+job.requested_time):
            return True
        
        if listFreeSpace is None:
            listFreeSpace = self.listFreeSpace
        
        free_procs = listFreeSpace.free_processors - job.requested_resources
        
        used_procs = self.bs.nb_res - free_procs
        
        power = used_procs*self.power_compute + free_procs*self.power_idle
        
        pc = (power <= self.powercap)
        if pc:
            return True
        if not canUseBudgetLeft:
            return pc

        energy_excess = (power-self.powercap)*job.requested_time
        
        if energy_excess <= self.budget_saved_measured-self.budget_reserved:
            self.budget_reserved += energy_excess
            job.reserved_power = (power-self.powercap)
            self.listJobReservedEnergy.append(job)
            return True
        return False



    def allocJobFCFS(self, job, current_time):
        #overrinding parent method
        
        if not self.estimate_if_job_fit_in_energy(job, current_time, canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured):
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
            
            fit_in_energy = self.estimate_if_job_fit_in_energy(job, j.estimate_finish_time, listFreeSpaceTemp, canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured)

            if fit_in_procs and fit_in_energy:
                alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, j.estimate_finish_time)
                #we find a valid allocation
                return (alloc, j.estimate_finish_time)
            
            previous_current_time = j.estimate_finish_time
        
        #We can be here only if a the budget end later that the last running job and that the current job does not fit in energy
        alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, self.budget_end)
        #we find a valid allocation
        return (alloc, self.budget_end)
    


    def allocJobBackfillWithEnergyCap(self, job, current_time):
        if job.requested_resources*job.requested_time*self.power_compute > self.energycap:
            return None
            
        for l in self.listFreeSpace.generator():
            if job.requested_resources <= l.res and job.requested_time <= l.length:
                alloc = self.listFreeSpace.assignJob(l, job, current_time)
                self.energycap -= job.requested_resources*job.requested_time*self.power_compute
                return alloc
        return None
        


    def findBackfilledAllocs(self, current_time, first_job_starttime):
        if self.reduce_powercap_to_save_energy:
            pc = self.powercap
            self.powercap -= self.budget_reserved/(first_job_starttime-current_time)
            
            ret = super(EasyEnergyBudget, self).findBackfilledAllocs(current_time, first_job_starttime)
            
            self.powercap = pc
            return ret
        elif self.estimate_energy_jobs_to_save_energy:
            self.allocJobBackfill_backup = self.allocJobBackfill
            self.allocJobBackfill = self.allocJobBackfillWithEnergyCap
            self.energycap = self.powercap*(first_job_starttime-current_time)- self.estimate_energy_running_jobs(current_time)-self.budget_reserved
            self.energycap_endtime = first_job_starttime
            
            ret = super(EasyEnergyBudget, self).findBackfilledAllocs(current_time, first_job_starttime)
            
            self.allocJobBackfill = self.allocJobBackfill_backup
            return ret
        else:
            return super(EasyEnergyBudget, self).findBackfilledAllocs(current_time, first_job_starttime)
    
    def allocBackFill(self, first_job, current_time):
        
        ret = super(EasyEnergyBudget, self).allocBackFill(first_job, current_time)
        
        if first_job in self.listJobReservedEnergy:
            #remove the energy reservation
            self.budget_reserved += first_job.reserved_power*first_job.requested_time
        
        
        return ret
        
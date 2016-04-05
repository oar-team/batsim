#/usr/bin/python3

from easyBackfill import *
from intervalContainer import *

from enum import Enum
class State(Enum):
    SwitchedON = 0
    SwitchedOFF = 1
    WantingToStartJob = 2
    SwitchingON = 3
    SwitchingOFF = 4


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

        if self.reduce_powercap_to_save_energy == self.estimate_energy_jobs_to_save_energy:
            assert False, "can't activate reduce_powercap_to_save_energy and estimate_energy_jobs_to_save_energy"


        super(EasyEnergyBudget, self).__init__(options)



    def onAfterBatsimInit(self):
        super(EasyEnergyBudget, self).onAfterBatsimInit()
        
        
        if self.opportunist_shutdown:
            self.nodes_states = IntervalOfStates(0,self.bs.nb_res-1,State.SwitchedON)
            for i in State:
                for j in State:
                    self.nodes_states.registerCallback(i, j, nodes_states_error)
            
            self.nodes_states.registerCallback(State.SwitchedON, State.SwitchingOFF, nodes_states_SwitchedON_SwitchingOFF)
            self.nodes_states.registerCallback(State.SwitchingOFF, State.SwitchedOFF, nodes_states_SwitchingOFF_SwitchedOFF)
            self.nodes_states.registerCallback(State.SwitchedOFF, State.SwitchingON, nodes_states_SwitchedOFF_SwitchingON)
            self.nodes_states.registerCallback(State.SwitchingON, State.SwitchedON, nodes_states_SwitchingON_SwitchedON)
            
            
            self.nodes_states.registerCallback(State.SwitchedON, State.WantingToStartJob, nodes_states_SwitchedON_WantingToStartJob)
            self.nodes_states.registerCallback(State.SwitchingOFF, State.WantingToStartJob, nodes_states_SwitchingOFF_WantingToStartJob)
            self.nodes_states.registerCallback(State.SwitchedOFF, State.WantingToStartJob, nodes_states_SwitchedOFF_WantingToStartJob)
            self.nodes_states.registerCallback(State.SwitchingON, State.WantingToStartJob, nodes_states_SwitchingON_WantingToStartJob)
            
            self.nodes_states.registerCallback(State.WantingToStartJob, State.SwitchedON, nodes_states_WantingToStartJob_SwitchedON)
            self.nodes_states.registerCallback(State.WantingToStartJob, State.SwitchedOFF, nodes_states_WantingToStartJob_SwitchedOFF)
            
            
            self.nodes_states.registerCallback(State.SwitchedOFF, State.SwitchingOFF, nodes_states_undo_changement)
            self.nodes_states.registerCallback(State.SwitchingOFF, State.SwitchingOFF, nodes_states_pass)
            
            #when we want to start a job that use non-switchedON nodes
            #we first wake up nodes and then start the job
            # the waiting jobs are put there:
            self.waiting_allocs = {}


    def onJobCompletion(self, job):
        try:
            self.listJobReservedEnergy.remove(job)
            stime = max(job.start_time, self.budget_start)
            etime = min(self.budget_end, job.start_time+job.requested_time)
            if not hasattr(job, "last_power_monitoring"):
                job.last_power_monitoring = stime
            self.budget_reserved -= job.reserved_power*(etime - job.last_power_monitoring)
            assert self.budget_reserved >= 0
            #assert sum([j.reserved_power*(min(self.budget_end, j.start_time+j.requested_time) - j.last_power_monitoring) for j in self.listJobReservedEnergy]) == self.budget_reserved
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

    def in_listRunningJob(self, job):
        for j in self.listRunningJob:
            if j.id == job.id:
                return True
        return False


    def onReportEnergyConsumed(self, consumed_energy):
        
        if self.monitoring_last_value is None:
            self.monitoring_last_value = consumed_energy
            self.monitoring_last_value_time = self.bs.time()
        
        delta_consumed_energy = consumed_energy - self.monitoring_last_value
        maximum_energy_consamble = (self.bs.time()-self.monitoring_last_value_time)*self.powercap
        self.budget_saved_measured += maximum_energy_consamble - delta_consumed_energy
        
        
        self.monitoring_last_value = consumed_energy
        self.monitoring_last_value_time = self.bs.time()
        
        for job in self.listJobReservedEnergy:
            assert self.in_listRunningJob(job)
            if not hasattr(job, "last_power_monitoring"):
                stime = max(job.start_time, self.budget_start) 
                job.last_power_monitoring = stime
            self.budget_reserved -= job.reserved_power*(self.bs.time()-job.last_power_monitoring)
            assert self.budget_reserved >= 0, "self.budget_reserved >= 0 ("+str(self.budget_reserved)
            job.last_power_monitoring = self.bs.time()
        
        self._schedule_jobs(self.bs.time())
        
        self.regiter_next_monitoring_event()



    def onNOP(self):
        current_time = self.bs.time()
        
        if current_time >= self.budget_start:
            self.bs.request_consumed_energy()
        
        if current_time >= self.budget_end:
            self._schedule_jobs(current_time)



    def onMachinePStateChanged(self, nodes, pstate):
        if pstate == self.pstate_switchoff:
            self.nodes_states.changeState(nodes[0], nodes[1], State.SwitchedOFF, self)
        elif pstate == self.pstate_switchon:
            self.nodes_states.changeState(nodes[0], nodes[1], State.SwitchedON, self)



    def _schedule_jobs(self, current_time):
        allocs = self.allocHeadOfList(current_time)
        
        if len(self.listWaitingJob) > 1:
            first_job = self.listWaitingJob.pop(0)
            allocs += self.allocBackFill(first_job, current_time)
            self.listWaitingJob.insert(0, first_job)
        
        #switchoff idle resources
        if self.opportunist_shutdown and (current_time < self.budget_end and self.budget_start <= current_time):
            for l in self.listFreeSpace.generator():
                self.nodes_states.changeState(l.first_res, l.last_res, State.SwitchingOFF, self)
        
        if self.opportunist_shutdown:
            for (j, (s,e)) in allocs:
                self.nodes_states.changeState(s, e, State.WantingToStartJob, (self, (j, (s,e))) )
        else:
            self.bs.start_jobs_continuous(allocs)


    #TODO: this function is useless
    #we already compute this value in findFutureAlloc
    def estimate_energy_running_jobs_and_idle_resources(self, current_time):
        e = 0.0
        starttime = max(self.budget_start, current_time)
        for j in self.listRunningJob:
            endtime = min(j.estimate_finish_time, self.budget_end)
            if endtime<starttime:
                continue
            e += j.requested_resources*self.power_compute*(endtime-starttime)
            e += j.requested_resources*self.power_idle*(endtime-self.budget_end)
        e += self.power_idle*(self.budget_end-starttime)
        return e


    def power_consumed(self, listFreeSpace, addUsedProc=0.0):
        free_procs = listFreeSpace.free_processors - addUsedProc
        
        used_procs = self.bs.nb_res - free_procs
        
        power = used_procs*self.power_compute + free_procs*self.power_idle
        
        return power


    def virtual_energy_excess(self, start, end, power):
        
        stime = max(start, self.budget_start)
        etime = min(self.budget_end, end)
        if stime>=etime:
            #we are outside energy budget
            return (0.0, 0.0)
        energy_excess = (power-self.powercap)*(etime-stime)
        
        return (energy_excess, (etime-stime))
        


    def estimate_if_job_fit_in_energy(self, job, start_time, listFreeSpace=None, canUseBudgetLeft=False, budgetVirtualySavedAtStartTime=0.0):
        """
        compute the power consumption of the cluster
        if <job> is not None then add the power consumption of this job as if it were running on the cluster.
        """
        #if the job does not cross the budget interval
        if not(start_time < self.budget_end and self.budget_start <= start_time+job.requested_time):
            return True
        
        if listFreeSpace is None:
            listFreeSpace = self.listFreeSpace
        
        power = self.power_consumed(listFreeSpace, job.requested_resources)
        
        pc = (power <= self.powercap)
        if pc:
            return True
        if not canUseBudgetLeft:
            return pc
        
        (energy_excess, energy_excess_len) = self.virtual_energy_excess(start_time, start_time+job.requested_time, power)
        if energy_excess <= budgetVirtualySavedAtStartTime+ self.budget_saved_measured-self.budget_reserved:
            self.budget_reserved += energy_excess
            job.reserved_power = (power-self.powercap)
            job.reserved_power_len = energy_excess_len
            self.listJobReservedEnergy.append(job)
            return True
        return False


    def unreserve_energy_job(self, job):
        if job in self.listJobReservedEnergy:
            self.listJobReservedEnergy.remove(job)
            energy_excess = job.reserved_power*job.reserved_power_len
            self.budget_reserved -= energy_excess


    def allocJobFCFS(self, job, current_time):
        #overrinding parent method
        if not self.estimate_if_job_fit_in_energy(job, current_time, canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured):
            return None
        res = super(EasyEnergyBudget, self).allocJobFCFS(job, current_time)
        if res is None:
            self.unreserve_energy_job(job)
        return res



    def allocJobBackfill(self, job, current_time):
        #overrinding parent method
        
        if not self.estimate_if_job_fit_in_energy(job, current_time):
            return None
        
        res = super(EasyEnergyBudget, self).allocJobBackfill(job, current_time)
        
        if res is None:
            self.unreserve_energy_job(job)
        
        return res



    def findAllocFuture(self, job):
        #overrinding parent method

        listFreeSpaceTemp = copy.deepcopy(self.listFreeSpace)
        
        if len(self.listRunningJob) == 0:
            #this condition happen if the powercap is so low that no job can be started now (perhaps some jobs will be backfilled, but we do not it yet)
            #we will ask batsim to wake up us at the end of the budget interval, just to be sure.
            if (self.bs.time() < self.budget_end) and not self.already_asked_to_awaken_at_end_of_budget:
                self.already_asked_to_awaken_at_end_of_budget = True
                self.bs.wake_me_up_at(self.budget_end)
            new_free_space_created_by_this_unallocation = listFreeSpaceTemp.firstItem
            #alloc = listFreeSpaceTemp.assignJob(listFreeSpaceTemp.firstItem, job, self.budget_end)
            #we find a valid allocation
            #return (alloc, self.budget_end)

        previous_current_time = self.bs.time()
        budgetVirtualySaved = 0.0
        for j in self.listRunningJob:
            if self.allow_FCFS_jobs_to_use_budget_saved_measured:
                power_consumed_until_end_of_j = self.power_consumed(listFreeSpaceTemp)
            
            new_free_space_created_by_this_unallocation = listFreeSpaceTemp.unassignJob(j)
            
            if self.allow_FCFS_jobs_to_use_budget_saved_measured:
                (energy, dontcare) = self.virtual_energy_excess(previous_current_time, j.estimate_finish_time, power_consumed_until_end_of_j)
                budgetVirtualySaved += energy

            fit_in_procs = job.requested_resources <= new_free_space_created_by_this_unallocation.res
            if fit_in_procs:
                fit_in_energy = self.estimate_if_job_fit_in_energy(job, j.estimate_finish_time, listFreeSpaceTemp, canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured, budgetVirtualySavedAtStartTime=budgetVirtualySaved)
                if fit_in_energy:
                    alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, j.estimate_finish_time)
                    #we find a valid allocation
                    assert j.estimate_finish_time > self.bs.time()
                    return (alloc, j.estimate_finish_time)
            
            previous_current_time = j.estimate_finish_time
        
        #We can be here only if the current job does not fit in energy
        if not self.allow_FCFS_jobs_to_use_budget_saved_measured:
            alloc = listFreeSpaceTemp.assignJob(listFreeSpaceTemp.firstItem, job, self.budget_end)
            return (alloc, self.budget_end)

            
        pjob = job.requested_resources*self.power_compute + (self.bs.nb_res - job.requested_resources) * self.power_idle
        pc = (self.powercap + (self.budget_saved_measured + self.budget_reserved)/(self.budget_end - self.bs.time()) )
        energy_excess = ( pjob - pc )* job.requested_time
        stime = energy_excess / (self.power_compute-self.power_idle)
        stime = min(stime + max(self.budget_start, self.bs.time()) , self.budget_end)
        if stime <= self.bs.time():
            stime = previous_current_time
        if stime <= self.bs.time():
            stime = self.bs.time()+1
        alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, stime)
        #we find a valid allocation
        return (alloc, stime)
    


    def allocJobBackfillWithEnergyCap(self, job, current_time):
        if job.requested_resources*job.requested_time*(self.power_compute-self.power_idle) > self.energycap:
            return None
            
        for l in self.listFreeSpace.generator():
            if job.requested_resources <= l.res and job.requested_time <= l.length:
                alloc = self.listFreeSpace.assignJob(l, job, current_time)
                self.energycap -= job.requested_resources*job.requested_time*(self.power_compute-self.power_idle)
                return alloc
        return None
        


    def findBackfilledAllocs(self, current_time, first_job_starttime):
        assert first_job_starttime>current_time
        if self.reduce_powercap_to_save_energy:
            pc = self.powercap
            self.powercap -= self.budget_reserved/(first_job_starttime-current_time)
            
            ret = super(EasyEnergyBudget, self).findBackfilledAllocs(current_time, first_job_starttime)
            
            self.powercap = pc
            return ret
        elif self.estimate_energy_jobs_to_save_energy:
            self.allocJobBackfill_backup = self.allocJobBackfill
            self.allocJobBackfill = self.allocJobBackfillWithEnergyCap
            #when comparing eneries to energycap, dont forget to sbustract idle energy
            self.energycap = self.powercap*(first_job_starttime-max(current_time, self.budget_start))- self.estimate_energy_running_jobs_and_idle_resources(current_time)-self.budget_reserved
            self.energycap_endtime = first_job_starttime
            
            ret = super(EasyEnergyBudget, self).findBackfilledAllocs(current_time, first_job_starttime)
            
            self.allocJobBackfill = self.allocJobBackfill_backup
            return ret
        else:
            assert False, "Impossible to get there"
    
    def allocBackFill(self, first_job, current_time):
        
        ret = super(EasyEnergyBudget, self).allocBackFill(first_job, current_time)
        
        #remove the energy reservation
        self.unreserve_energy_job(first_job)
        
        return ret


    def add_alloc_to_waiting_allocs(self, alloc):
        if not(alloc is None):
            self.waiting_allocs[alloc[0]] = alloc[1]

def nodes_states_error(self, start, end, fromState, toState):
    assert False, "Unpossible transition: ["+str(start)+"-"+str(end)+"] "+str(fromState)+" => "+str(toState)
def nodes_states_pass(self, start, end, fromState, toState):
    pass

def nodes_states_undo_changement(self, start, end, fromState, toState):
    self.nodes_states.changeState(start, end, fromState, self)


def nodes_states_SwitchedON_SwitchingOFF(self, start, end, fromState, toState):
    #send pstate change
    pstates_to_change = [(self.pstate_switchoff, (start, end))]
    self.bs.change_pstates(pstates_to_change)

def nodes_states_SwitchingOFF_SwitchedOFF(self, start, end, fromState, toState):
        pass

def nodes_states_SwitchedOFF_SwitchingON(self, start, end, fromState, toState):
        #send pstate
        pstates_to_change = [(self.pstate_switchon, (start, end))]
        self.bs.change_pstates(pstates_to_change)

def nodes_states_SwitchingON_SwitchedON(self, start, end, fromState, toState):
        pass



def nodes_states_SwitchedON_WantingToStartJob((self, alloc), start, end, fromState, toState):
        #wait job
        self.add_alloc_to_waiting_allocs(alloc)
        #changeState to SwitchedON
        self.nodes_states.changeState(start, end, State.SwitchedON, self)

def nodes_states_SwitchingOFF_WantingToStartJob((self, alloc), start, end, fromState, toState):
        #wait job
        self.add_alloc_to_waiting_allocs(alloc)

def nodes_states_SwitchedOFF_WantingToStartJob((self, alloc), start, end, fromState, toState):
        #send ON
        pstates_to_change = [(self.pstate_switchon, (start, end))]
        self.bs.change_pstates(pstates_to_change)
        #wait job
        self.add_alloc_to_waiting_allocs(alloc)

def nodes_states_SwitchingON_WantingToStartJob((self, alloc), start, end, fromState, toState):
        #wait job
        self.add_alloc_to_waiting_allocs(alloc)


def nodes_states_WantingToStartJob_SwitchedON(self, start, end, fromState, toState):
        #start waiting jobs
        allocs_to_start = []
        for j, (s,e) in self.waiting_allocs.iteritems():
            if s > end or e < start:
                continue
            #the first part of the "or" is an optimisation
            if (start <= s and e <= end) or self.nodes_states.contains(s, e, State.SwitchedON):
                allocs_to_start.append((j, (s,e)))
                #the job is already assign in the freespace structure
                #we need to resort it in listRunningJob
                self.listRunningJob.remove(j)
                #update ending time
                j.start_time = self.bs.time()
                j.estimate_finish_time = j.requested_time + j.start_time
                self.listRunningJob.add(j)
        for a in allocs_to_start:
            del self.waiting_allocs[a[0]]
        self.bs.start_jobs_continuous(allocs_to_start)


def nodes_states_WantingToStartJob_SwitchedOFF(self, start, end, fromState, toState):
        #changeState to WantingToStartJob (self, None)
        self.nodes_states.changeState(start, end, State.WantingToStartJob, (self, None))






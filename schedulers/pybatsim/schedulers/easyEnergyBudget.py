#/usr/bin/python3

from easyBackfill import *
from intervalContainer import *
import math

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
        
        self.budget_total = float(options["budget_total"])
        self.budget_start = float(options["budget_start"])#the budget limitation start at this time included
        self.budget_end = float(options["budget_end"])#the budget end just before this time
        
        self.allow_FCFS_jobs_to_use_budget_saved_measured = options["allow_FCFS_jobs_to_use_budget_saved_measured"]
        
        self.reduce_powercap_to_save_energy = options["reduce_powercap_to_save_energy"]
        self.estimate_energy_jobs_to_save_energy = not self.reduce_powercap_to_save_energy
        
        
        self.budget_saved_measured = 0.0
        self.budget_reserved = 0.0
        self.listJobReservedEnergy = []
        
        self.power_idle = float(options["power_idle"])
        self.power_compute = float(options["power_compute"])
        
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
            assert self.budget_reserved >= 0.0, str(self.budget_reserved)+" >= 0.0 "
            self.listJobReservedEnergy.remove(job)
            etime = min(self.budget_end, job.start_time+job.requested_time)
            if not hasattr(job, "last_power_monitoring"):
                stime = max(job.start_time, self.budget_start)
                job.last_power_monitoring = stime
            
            
            if job.start_time >= self.budget_end:
                #due to the shutdown, some job can start after budget_end
                #and be in the reservation list.
                 self.budget_reserved -= job.reserved_power*job.reserved_power_len
            else:
                assert etime >= job.last_power_monitoring, str(etime)+" >= "+str(job.last_power_monitoring)
                self.budget_reserved -= job.reserved_power*(etime - job.last_power_monitoring)
            assert self.budget_reserved >= 0.0, str(self.budget_reserved)+" >= 0.0 "+str(job)
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
        assert delta_consumed_energy >= 0
        maximum_energy_consamble = (self.bs.time()-self.monitoring_last_value_time)*self.powercap
        self.budget_saved_measured += maximum_energy_consamble - delta_consumed_energy
        
        
        self.monitoring_last_value = consumed_energy
        self.monitoring_last_value_time = self.bs.time()
        
        for job in self.listJobReservedEnergy:
            assert self.in_listRunningJob(job)
            if not hasattr(job, "last_power_monitoring"):
                stime = max(job.start_time, self.budget_start) 
                job.last_power_monitoring = stime
            etime = min(self.budget_end, self.bs.time())
            assert etime >= job.last_power_monitoring
            self.budget_reserved -= job.reserved_power*(etime-job.last_power_monitoring)
            assert self.budget_reserved >= 0, "self.budget_reserved >= 0 ("+str(self.budget_reserved)
            job.last_power_monitoring = etime
        
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


    def power_consumed(self, listFreeSpace, addUsedProc=0):
        free_procs = float(listFreeSpace.free_processors - addUsedProc)
        assert free_procs >= 0.0, str(addUsedProc)+" / "+str(listFreeSpace.free_processors)
        used_procs = float(self.bs.nb_res - free_procs)
        assert used_procs >= 0.0
        
        power = used_procs*self.power_compute + free_procs*self.power_idle
        
        return power


    def virtual_energy_excess(self, start, end, power):
        
        stime = max(start, self.budget_start)
        etime = min(self.budget_end, end)
        if stime>=etime:
            #we are outside energy budget
            return (0.0, 0.0)
        energy_excess = (power-self.powercap)*float(etime-stime)
        return (energy_excess, float(etime-stime))
        


    def estimate_if_job_fit_in_energy(self, job, start_time, listFreeSpace=None, canUseBudgetLeft=False, budgetVirtualySavedAtStartTime=0.0):
        """
        compute the power consumption of the cluster
        if <job> is not None then add the power consumption of this job as if it were running on the cluster.
        
        WARNING: if the job fit, it do the enerrgy reservation!!!!!!!
        """
        #print "==== estimate_if_job_fit_in_energy", job, start_time, canUseBudgetLeft, budgetVirtualySavedAtStartTime
        #if the job does not cross the budget interval
        if not(start_time < self.budget_end and self.budget_start <= start_time+job.requested_time):
            return True
        
        if listFreeSpace is None:
            listFreeSpace = self.listFreeSpace
        
        if listFreeSpace.free_processors < job.requested_resources:
            return False
        
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
            assert self.budget_reserved >= 0, str(job)+"  "+str(self.budget_reserved)+"  "+str(energy_excess)


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

        listFreeSpaceTemp = self.listFreeSpace.copy()
        
        if len(self.listRunningJob) == 0:
            #this condition happen if the powercap is so low that no job can be started now (perhaps some jobs will be backfilled, but we do not it yet)
            #we will ask batsim to wake up us at the end of the budget interval, just to be sure.
            if (self.bs.time() < self.budget_end) and not self.already_asked_to_awaken_at_end_of_budget:
                self.already_asked_to_awaken_at_end_of_budget = True
                self.bs.wake_me_up_at(self.budget_end)

        previous_current_time = self.bs.time()
        budgetVirtualySaved = 0.0
        first_time_fit_in_proc = self.bs.time()
        for j in self.listRunningJob:
            if self.allow_FCFS_jobs_to_use_budget_saved_measured:
                power_consumed_until_end_of_j = self.power_consumed(listFreeSpaceTemp)
            
            new_free_space_created_by_this_unallocation = listFreeSpaceTemp.unassignJob(j)
            
            
            assert j.estimate_finish_time >= previous_current_time, str(j.estimate_finish_time)+" >= "+str(previous_current_time)
            
            if self.allow_FCFS_jobs_to_use_budget_saved_measured:
                (energy, dontcare) = self.virtual_energy_excess(previous_current_time, j.estimate_finish_time, power_consumed_until_end_of_j)
                budgetVirtualySaved += energy

            fit_in_procs = job.requested_resources <= new_free_space_created_by_this_unallocation.res
            if fit_in_procs:
                if first_time_fit_in_proc == self.bs.time():
                    first_time_fit_in_proc = j.estimate_finish_time
                fit_in_energy = self.estimate_if_job_fit_in_energy(job, j.estimate_finish_time, listFreeSpaceTemp, canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured, budgetVirtualySavedAtStartTime=budgetVirtualySaved)
                if fit_in_energy:
                    alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, j.estimate_finish_time)
                    #we find a valid allocation
                    return (alloc, j.estimate_finish_time)
            
            previous_current_time = j.estimate_finish_time
        
        #We can be here only if the current job does not fit in energy
        assert listFreeSpaceTemp.firstItem.nextt is None
        if not self.allow_FCFS_jobs_to_use_budget_saved_measured:
            alloc = listFreeSpaceTemp.assignJob(listFreeSpaceTemp.firstItem, job, max(self.budget_end, first_time_fit_in_proc))
            return (alloc, max(self.budget_end, first_time_fit_in_proc))

        
        #we know that the job fit in proc from first_time_fit_in_proc
        #we know that the job does not fit in energy up to previous_current_time
        assert previous_current_time <= self.budget_end
        assert first_time_fit_in_proc <= previous_current_time
        
        #we have to find the first time that the job fit in energy
        
        #we have 2 cases, either the job end before budget_end, or after
        #let's determine in which case we are by looking at if we can start the job if it end just before budget_end
        start_job_before_end_budget = self.budget_end - job.requested_time
        power_idle_save = self.powercap - float(self.bs.nb_res) * self.power_idle
        
        can_start = True
        if start_job_before_end_budget < previous_current_time:
            can_start = False
        else:
            can_start = self.estimate_if_job_fit_in_energy(
                            job, start_job_before_end_budget,
                            listFreeSpaceTemp,
                            canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured,
                            budgetVirtualySavedAtStartTime=
                                (budgetVirtualySaved+power_idle_save*(start_job_before_end_budget-previous_current_time)))
            if can_start:
                self.unreserve_energy_job(job)
        
        budget_already_saved = budgetVirtualySaved + self.budget_saved_measured-self.budget_reserved

        minstart = max(previous_current_time, self.budget_start)
        
        power = self.power_consumed(listFreeSpaceTemp, job.requested_resources)
        
        if can_start:#job will end before budget_end
            assert power_idle_save > 0.0, str(power_idle_save)
            (energy_excess, dontcare) = self.virtual_energy_excess(start_job_before_end_budget, start_job_before_end_budget+job.requested_time, power)
            
            assert energy_excess > budget_already_saved
            # t* power_idle_save == (energy_excess - budget_already_saved)
            t = (energy_excess - budget_already_saved) / power_idle_save
            
            start_job = math.ceil(t + minstart)#the ceil remove float problems
            
            budgetVirtualySaved += t*power_idle_save
            
            assert start_job <= start_job_before_end_budget,  str(start_job)+" <= "+str(start_job_before_end_budget)+" // "+str(self.bs.time())
            
        else: #job will end after budget_end
            minstart = max(previous_current_time, self.budget_start)
            #in this case energy_excess = (power-self.powercap)*((self.budget_end-previous_current_time)-t)
            #we want: t * power_idle_save == (power-self.powercap)*((self.budget_end-previous_current_time)-t) - budget_already_saved
            t = ( (power-self.powercap)*(self.budget_end-minstart) - budget_already_saved ) / ( power_idle_save+(power-self.powercap) )
            
            start_job = math.ceil(t + minstart)#the ceil remove float problems
            
            budgetVirtualySaved += t*power_idle_save
            
            if start_job > self.budget_end:
                assert budget_already_saved < 0.0 or power_idle_save < 0.0, str(budget_already_saved)+" // "+str(start_job)+" // "+str(self.budget_end)
                start_job = self.budget_end
            
        #this test is not only an assert: it also makes the energy reservation for the job.
        if not(self.estimate_if_job_fit_in_energy(job, start_job, listFreeSpaceTemp, canUseBudgetLeft=self.allow_FCFS_jobs_to_use_budget_saved_measured, budgetVirtualySavedAtStartTime=budgetVirtualySaved)):
            assert False
            return None
        
        
        alloc = listFreeSpaceTemp.assignJob(listFreeSpaceTemp.firstItem, job, start_job)
        assert start_job > self.bs.time()
        return (alloc, start_job)


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
            job = alloc[0]
            if not(job in self.waiting_allocs) and hasattr(job, "reserved_power"):
                job.reserved_power_real = job.reserved_power
                job.reserved_power = 0.0
            self.waiting_allocs[job] = alloc[1]

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
                if hasattr(j, "reserved_power_real"):
                    j.reserved_power = j.reserved_power_real
                    if hasattr(j, "last_power_monitoring"):
                        del j.last_power_monitoring
        for a in allocs_to_start:
            del self.waiting_allocs[a[0]]
        self.bs.start_jobs_continuous(allocs_to_start)


def nodes_states_WantingToStartJob_SwitchedOFF(self, start, end, fromState, toState):
        #changeState to WantingToStartJob (self, None)
        self.nodes_states.changeState(start, end, State.WantingToStartJob, (self, None))






#/usr/bin/python3

from batsim.batsim import BatsimScheduler, Batsim

import sys
import os
from random import sample
from sortedcontainers import SortedSet
from enum import Enum

# This scheduler is a FCFS sleeper one :
#   - Released jobs are pushed in the back of one single queue
#   - Two jobs cannot be executed on the same machine at the same time
#   - Only the job at the head of the queue can be allocated
#     - If the job is too big (will never fit the machine), it is rejected
#     - If the job can fit the machine now, it is allocated (and run) instantly
#     - If the job cannot fit the machine now
#       - If the job cannot fit because of other jobs, the unused machines are switched OFF
#       - Else (if the job cannot fit because of sleeping machines), those machines are switched ON

# Let us assume all machines have the following pstates (corresponding to file energy_platform_homogeneous.xml)
class PState(Enum):
    ComputeFast = 0
    ComputeMedium = 1
    ComputeSlow = 2
    Sleep = 3
    SwitchingOFF = 4
    SwitchingON = 5

class State(Enum):
    Computing = 0
    Idle = 1
    Sleeping = 2
    SwitchingON = 3
    SwitchingOFF = 4

class FcfsSchedSleep(BatsimScheduler):

    def __init__(self, options):
        self.options = options

    def onAfterBatsimInit(self):
        self.nb_completed_jobs = 0

        self.jobs_res = {}
        self.jobs_completed = []
        self.jobs_waiting = []

        self.sched_delay = 0.0

        self.open_jobs = []

        self.computing_machines = SortedSet()
        self.idle_machines = SortedSet(range(self.bs.nb_res))
        self.sleeping_machines = SortedSet()
        self.switching_ON_machines = SortedSet()
        self.switching_OFF_machines = SortedSet()

        self.machines_states = {int(i):State.Idle.value for i in range(self.bs.nb_res)}
        print("machines_states", self.machines_states)

    def scheduleJobs(self):
        print('\n\n\n\n')
        print('open_jobs = ', self.open_jobs)
        print('jobs_res = ', self.jobs_res)

        print('computingM = ', self.computing_machines)
        print('idleM = ', self.idle_machines)
        print('sleepingM = ', self.sleeping_machines)
        print('switchingON_M = ', self.switching_ON_machines)
        print('switchingOFF_M = ', self.switching_OFF_machines)

        scheduled_jobs = []
        pstates_to_change = {}
        loop = True

        # If there is a job to schedule
        while loop and self.open_jobs:
            job = self.open_jobs[0]
            nb_res_req = job.requested_resources

            if nb_res_req > self.bs.nb_res: # Job too big -> rejection
                sys.exit("Rejection unimplemented")

            elif nb_res_req <= len(self.idle_machines): # Job fits now -> allocation
                res = self.idle_machines[:nb_res_req]
                self.jobs_res[job.id] = res
                scheduled_jobs.append(job)
                for r in res: # Machines' states update
                    self.idle_machines.remove(r)
                    self.computing_machines.add(r)
                    self.machines_states[r] = State.Computing.value
                self.open_jobs.remove(job)

            else: # Job can fit on the machine, but not now
                loop = False
                print("############ Job does not fit now ############")
                nb_not_computing_machines = self.bs.nb_res - len(self.computing_machines)
                print("nb_res_req = ", nb_res_req)
                print("nb_not_computing_machines = ", nb_not_computing_machines)
                if nb_res_req <= nb_not_computing_machines: # The job could fit if more machines were switched ON
                    # Let us switch some machines ON in order to run the job
                    nb_res_to_switch_ON = nb_res_req - len(self.idle_machines) - len(self.switching_ON_machines)
                    print("nb_res_to_switch_ON = ", nb_res_to_switch_ON)
                    if nb_res_to_switch_ON > 0: # if some machines need to be switched ON now
                        nb_switch_ON = min(nb_res_to_switch_ON, len(self.sleeping_machines))
                        if nb_switch_ON > 0: # If some machines can be switched ON now
                            res = self.sleeping_machines[:nb_switch_ON]
                            for r in res: # Machines' states update + pstate change request
                                self.sleeping_machines.remove(r)
                                self.switching_ON_machines.add(r)
                                self.machines_states[r] = State.SwitchingON.value
                                pstates_to_change[r] = PState.ComputeFast.value
                else: # The job cannot fit now because of other jobs
                    # Let us put all idle machines to sleep
                    for r in self.idle_machines:
                        self.switching_OFF_machines.add(r)
                        self.machines_states[r] = State.SwitchingOFF.value
                        pstates_to_change[r] = PState.Sleep.value
                    self.idle_machines = SortedSet()

        # if there is nothing to do, let us put all idle machines to sleep
        if not self.open_jobs:
            for r in self.idle_machines:
                self.switching_OFF_machines.add(r)
                self.machines_states[r] = State.SwitchingOFF.value
                pstates_to_change[r] = PState.Sleep.value
            self.idle_machines = SortedSet()

        # update time
        self.bs.consume_time(self.sched_delay)

        # send to uds
        self.bs.start_jobs(scheduled_jobs, self.jobs_res)
        self.bs.change_pstates(pstates_to_change)



    def onJobSubmission(self, job):
        self.open_jobs.append(job)
        self.scheduleJobs()

    def onJobCompletion(self, job):
        for res in self.jobs_res[job.id]:
            self.idle_machines.add(res)
            self.computing_machines.remove(res)
            self.machines_states[res] = State.Idle.value
        self.scheduleJobs()

    def onMachinePStateChanged(self, machine, new_pstate):
        if (new_pstate == PState.ComputeFast.value) or (new_pstate == PState.ComputeMedium.value) or (new_pstate == PState.ComputeSlow.value): # switched to a compute pstate
            if self.machines_states[machine] == State.SwitchingON.value:
                self.switching_ON_machines.remove(machine)
                self.idle_machines.add(machine)
                self.machines_states[machine] = State.Idle.value
            else:
                sys.exit("Unhandled case: a machine switched to a compute pstate but was not switching ON")
        elif new_pstate == PState.Sleep.value:
            if self.machines_states[machine] == State.SwitchingOFF.value:
                self.switching_OFF_machines.remove(machine)
                self.sleeping_machines.add(machine)
                self.machines_states[machine] = State.Sleeping.value
            else:
                sys.exit("Unhandled case: a machine switched to a sleep pstate but was not switching OFF")
        else:
            sys.exit("Switched to an unhandled pstate: " + str(new_pstate))
        
        self.scheduleJobs()
    
    
    
    
    
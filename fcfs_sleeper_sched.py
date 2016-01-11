#/usr/bin/python3

import struct
import socket
import sys
import os
import json
import random
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

def read_bat_msg(connection):
    lg_str = connection.recv(4)

    if not lg_str:
        print("connection is closed by batsim core")
        exit(1)

    lg = struct.unpack("i",lg_str)[0]
    msg = connection.recv(lg).decode()
    print('from batsim : %r' % msg)
    sub_msgs = msg.split('|')
    data = sub_msgs[0].split(":")
    version = int(data[0])
    now = float(data[1])

    print("version: ", version, " now: ", now)

    jobs_submitted = []
    new_jobs_completed = []
    pstate_changed = {}

    for i in range(1, len(sub_msgs)):
        data = sub_msgs[i].split(':')
        if data[1] == 'S':
            jobs_submitted.append( int(data[2]) )
        elif data[1] == 'C':
            new_jobs_completed.append(int(data[2]))
        elif data[1] == 'p':
            data2 = data[2].split('=')
            pstate_changed[int(data2[0])] = int(data2[1])
        elif data[1] == 'e':
            consumed_energy = float(data[2])
            print("Consumed energy is " + str(consumed_energy) + " J")
        else:
            raise Exception("Unknow submessage type" + data[1] )

    return (now, jobs_submitted, new_jobs_completed, pstate_changed)

def send_bat_msg(connection, now, jids_toLaunch, jobs, pstates_to_change):
    didSomething = False
    msg = "1:" + str(now)

    if jids_toLaunch: # if jobs are to be launched
        didSomething = True
        msg += '|' + str(now) + ":J:"
        for jid in jids_toLaunch:
            msg += str(jid) + "="
            for r in jobs[jid]:
               msg += str(r) + ","
            msg = msg[:-1] + ";" # replace last comma by semicolon separtor between jobs
        msg = msg[:-1] # remove last semicolon

    if pstates_to_change: # if pstates are to be changed
        didSomething = True
        parts = [str(p) + "=" + str(pstates_to_change[p]) for p in pstates_to_change] # create machine=new_pstate string parts
        for part in parts:
            msg += "|" + str(now) + ":P:" + part

    if not didSomething:
        if random.choice([0,1]) == 0:
            msg += "|" + str(now) +":N"
        else:
            msg += "|" + str(now) +":E"

    print(msg)
    lg = struct.pack("i",int(len(msg)))
    connection.sendall(lg)
    connection.sendall(msg.encode())

def load_json_workload_profile(filename):
    wkp_file = open(filename)
    wkp = json.load(wkp_file)
    return wkp["jobs"], wkp["nb_res"]



###
#
#

server_address = '/tmp/bat_socket'

json_jobs, nb_res = load_json_workload_profile(sys.argv[1])
print("nb_res", nb_res)

jobs_res_req = { j["id"]: j["res"] for j in json_jobs}
print(jobs_res_req[1])

nb_jobs = len(jobs_res_req)
print("nb_jobs", nb_jobs)

nb_completed_jobs = 0

jobs_res = {}
jobs_completed = []
jobs_waiting = []

sched_delay = 0.0

open_jobs = []

computing_machines = SortedSet()
idle_machines = SortedSet(range(nb_res))
sleeping_machines = SortedSet()
switching_ON_machines = SortedSet()
switching_OFF_machines = SortedSet()

machines_states = {int(i):State.Idle.value for i in range(nb_res)}
print("machines_states", machines_states)

# Network connection to the simulator
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
print('connecting to', server_address)
try:
    sock.connect(server_address)
    print('connected')
except socket.error:
    print("socket error")
    sys.exit(1)

# Infinite loop to handle jobs
while True:
    now_str, jobs_submitted, jobs_completed, pstates_changed = read_bat_msg(sock)

    for job in jobs_submitted:
        open_jobs.append(job)

    for job in jobs_completed:
        for res in jobs_res[job]:
            idle_machines.add(res)
            computing_machines.remove(res)
            machines_states[res] = State.Idle.value

    for machine in pstates_changed:
        new_pstate = pstates_changed[machine]
        if (new_pstate == PState.ComputeFast.value) or (new_pstate == PState.ComputeMedium.value) or (new_pstate == PState.ComputeSlow.value): # switched to a compute pstate
            if machines_states[machine] == State.SwitchingON.value:
                switching_ON_machines.remove(machine)
                idle_machines.add(machine)
                machines_states[machine] = State.Idle.value
            else:
                sys.exit("Unhandled case: a machine switched to a compute pstate but was not switching ON")
        elif new_pstate == PState.Sleep.value:
            if machines_states[machine] == State.SwitchingOFF.value:
                switching_OFF_machines.remove(machine)
                sleeping_machines.add(machine)
                machines_states[machine] = State.Sleeping.value
            else:
                sys.exit("Unhandled case: a machine switched to a sleep pstate but was not switching OFF")
        else:
            sys.exit("Switched to an unhandled pstate: " + str(new_pstate))

    print('\n\n\n\n')
    print('open_jobs = ', open_jobs)
    print('jobs_res = ', jobs_res)

    print('computingM = ', computing_machines)
    print('idleM = ', idle_machines)
    print('sleepingM = ', sleeping_machines)
    print('switchingON_M = ', switching_ON_machines)
    print('switchingOFF_M = ', switching_OFF_machines)

    scheduled_jobs = []
    pstates_to_change = {}
    loop = True

    # If there is a job to schedule
    while loop and open_jobs:
        job = open_jobs[0]
        nb_res_req = jobs_res_req[job]

        if nb_res_req > nb_res: # Job too big -> rejection
            sys.exit("Rejection unimplemented")

        elif nb_res_req <= len(idle_machines): # Job fits now -> allocation
            res = idle_machines[:nb_res_req]
            jobs_res[job] = res
            scheduled_jobs.append(job)
            for r in res: # Machines' states update
                idle_machines.remove(r)
                computing_machines.add(r)
                machines_states[r] = State.Computing.value
            open_jobs.remove(job)

        else: # Job can fit on the machine, but not now
            loop = False
            print("############ Job does not fit now ############")
            nb_not_computing_machines = nb_res - len(computing_machines)
            print("nb_res_req = ", nb_res_req)
            print("nb_not_computing_machines = ", nb_not_computing_machines)
            if nb_res_req <= nb_not_computing_machines: # The job could fit if more machines were switched ON
                # Let us switch some machines ON in order to run the job
                nb_res_to_switch_ON = nb_res_req - len(idle_machines) - len(switching_ON_machines)
                print("nb_res_to_switch_ON = ", nb_res_to_switch_ON)
                if nb_res_to_switch_ON > 0: # if some machines need to be switched ON now
                    nb_switch_ON = min(nb_res_to_switch_ON, len(sleeping_machines))
                    if nb_switch_ON > 0: # If some machines can be switched ON now
                        res = sleeping_machines[:nb_switch_ON]
                        for r in res: # Machines' states update + pstate change request
                            sleeping_machines.remove(r)
                            switching_ON_machines.add(r)
                            machines_states[r] = State.SwitchingON.value
                            pstates_to_change[r] = PState.ComputeFast.value
            else: # The job cannot fit now because of other jobs
                # Let us put all idle machines to sleep
                for r in idle_machines:
                    switching_OFF_machines.add(r)
                    machines_states[r] = State.SwitchingOFF.value
                    pstates_to_change[r] = PState.Sleep.value
                idle_machines = SortedSet()

    # if there is nothing to do, let us put all idle machines to sleep
    if not open_jobs:
        for r in idle_machines:
            switching_OFF_machines.add(r)
            machines_states[r] = State.SwitchingOFF.value
            pstates_to_change[r] = PState.Sleep.value
        idle_machines = SortedSet()

    # update time
    now = float(now_str) + sched_delay

    # send to uds
    send_bat_msg(sock, now, scheduled_jobs, jobs_res, pstates_to_change)

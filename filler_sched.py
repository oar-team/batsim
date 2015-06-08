#/usr/bin/python3

import struct
import socket
import sys
import os
import json
from random import sample
from sortedcontainers import SortedSet

def create_uds(uds_name):
    # Make sure the socket does not already exist
    try:
        os.unlink(uds_name)
    except OSError:
        if os.path.exists(uds_name):
            raise

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    
    # Bind the socket to the port
    print('Starting up on', uds_name)
    sock.bind(uds_name)

    # Listen for incoming connections
    sock.listen(1)

    return sock

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
    for i in range(1, len(sub_msgs)):
        data = sub_msgs[i].split(':')
        if data[1] == 'S':
            jobs_submitted.append( int(data[2]) )
        elif data[1] == 'C':
            new_jobs_completed.append(int(data[2]))
        else:
            raise Exception("Unknow submessage type" + data[1] )  

    return (now, jobs_submitted, new_jobs_completed)

def send_bat_msg(connection, now, jids_toLaunch, jobs):
    msg = "0:" + str(now) + "|"
    if jids_toLaunch:
        msg +=  str(now) + ":J:" 
        for jid in jids_toLaunch:
            msg += str(jid) + "="
            for r in jobs[jid]:
               msg += str(r) + ","
            msg = msg[:-1] + ";" # replace last comma by semicolon separtor between jobs
        msg = msg[:-1] # remove last semicolon

    else: #Do nothing        
        msg +=  str(now) +":N"

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

sched_delay = 5.0

openJobs = set()
availableResources = SortedSet(range(nb_res))
previousAllocations = dict()

## 
# uds creation and waiting for connection
#
sock = create_uds(server_address)
print("waiting for a connection")
connection, client_address = sock.accept()
  
while True:
    now_str, jobs_submitted, jobs_completed = read_bat_msg(connection)

    for job in jobs_submitted:
        openJobs.add(job)

    for job in jobs_completed:
        for res in previousAllocations[job]:
            availableResources.add(res)
        previousAllocations.pop(job)

    scheduledJobs = []

    print('openJobs = ', openJobs)
    print('available = ', availableResources)
    print('previous = ', previousAllocations)
    
    # Iterating over a copy to be able to remove jobs from openJobs at traversal
    for job in set(openJobs):
        nb_res_req = jobs_res_req[job]

        if nb_res_req <= len(availableResources):
            res = availableResources[:nb_res_req]
            jobs_res[job] = res
            previousAllocations[job] = res
            scheduledJobs.append(job)

            for r in res:
                availableResources.remove(r)

            openJobs.remove(job)

    # update time 
    now = float(now_str) + sched_delay

    # send to uds
    send_bat_msg(connection, now, scheduledJobs, jobs_res)

    print('openJobs = ', openJobs)
    print('available = ', availableResources)
    print('previous = ', previousAllocations)
    print('')

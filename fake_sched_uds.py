import struct
import socket
import sys
import os
import json
from random import sample

def create_uds(uds_name):
    # Make sure the socket does not already exist
    try:
        os.unlink(uds_name)
    except OSError:
        if os.path.exists(uds_name):
            raise

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    
    # Bind the socket to the port
    print >>sys.stderr, 'starting up on %s' % uds_name
    sock.bind(uds_name)

    # Listen for incoming connections
    sock.listen(1)

    return sock

def read_bat_msg(connection):
    lg_str = connection.recv(4)
    
    if not lg_str:
        print "connection is closed by batsim core"
        exit(1)
        
    #print 'from client (lg_str): %r' % lg_str
    lg = struct.unpack("i",lg_str)[0]
    #print 'size msg to recv %d' % lg
    msg = connection.recv(lg)
    print 'from batsim : %r' % msg
    sub_msgs = msg.split('|')
    data = sub_msgs[-1].split(":")
    if data[2] != 'T':
        raise Exception("Terminal submessage must be T type")
    now = float(data[1])

    jobs_submitted = []
    new_jobs_completed = []
    for i in range(len(sub_msgs)-1):
        data = sub_msgs[i].split(':')
        if data[2] == 'S':
            jobs_submitted.append( int(data[3]) )
        elif data[2] == 'C':
            new_jobs_completed.append(int(data[3]))
        else:
            raise Exception("Unknow submessage type" + data[2] )  

    return (now, jobs_submitted, new_jobs_completed)

def send_bat_msg(connection, now, jids_toLaunch, jobs):
    msg = "0:" + str(now)
    if jids_toLaunch:
        msg += ":J:" 
        for jid in jids_toLaunch:
            msg += str(jid) + "="
            for r in jobs[jid]:
               msg += str(r) + ","
            msg = msg[:-1] + ";" # replace last comma by semicolon separtor between jobs
        msg = msg[:-1] # remove last semicolon

    else: #Do nothing        
        msg += ":N"

    print msg
    lg = struct.pack("i",int(len(msg)))
    connection.sendall(lg)
    connection.sendall(msg)

def load_json_workload_profile(filename):
    wkp_file = open(filename)
    wkp = json.load(wkp_file)
    return wkp["jobs"], wkp["nb_res"] 



###
#
#

server_address = '/tmp/bat_socket'

json_jobs, nb_res = load_json_workload_profile(sys.argv[1])
print "nb_res", nb_res

jobs_res_req = { j["id"]: j["res"] for j in json_jobs} 

nb_jobs = len(jobs_res_req)
print "nb_jobs", nb_jobs

nb_completed_jobs = 0

jobs_res = {}
jobs_completed = []
jobs_waiting = []

sched_delay = 5.0


## 
# uds creation and waiting for connection
#
sock = create_uds(server_address)
print "waiting for a connection"
connection, client_address = sock.accept()
  
while nb_completed_jobs < nb_jobs:
    # read from uds
    now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
    
    # fake scheduler
    for job_id in jobs_submitted:
        #
        # always schedule jobs !
        # random resources assignment !
        # 
        nb_res_req = jobs_res_req[job_id]
        res = sample(range(nb_res), nb_res_req)
        jobs_res[job_id] = res

    # update time 
    now = float(now_str) + sched_delay

    # send to uds
    send_bat_msg(connection, now, jobs_submitted, jobs_res)

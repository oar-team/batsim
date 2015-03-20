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
    data = sub_msgs[0].split(":")
    version = int(data[0])
    now = float(data[1])
    
    print "version: ", version, " now: ", now

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

# Submission of 1, which is instantly allocated
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [1])
assert(new_jobs_completed == [])
jobs_res[1] = [0,1,2,3]
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)

# Completion of 1, nothing to do
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [])
assert(new_jobs_completed == [1])
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)

# Submission of 2, which is instantly allocated
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [2])
assert(new_jobs_completed == [])
jobs_res[2] = [0,1,2,3]
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)


# Completion of 2, nothing to do
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [])
assert(new_jobs_completed == [2])
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)

# Submission of 3, which is instantly allocated
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [3])
assert(new_jobs_completed == [])
jobs_res[3] = [0,1,2,3]
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)

# Submission of 4, which is instantly allocated whereas resources are busy
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [4])
assert(new_jobs_completed == [])
jobs_res[4] = [0,1,2,3]
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)

# Completion of 3, nothing to do
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [])
assert(new_jobs_completed == [3])
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)

# Completion of 4, nothing to do
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)
assert(jobs_submitted == [])
assert(new_jobs_completed == [4])
send_bat_msg(connection, float(now_str), jobs_submitted, jobs_res)
  
now_str, jobs_submitted, new_jobs_completed = read_bat_msg(connection)


'''
0:10.000015|10.000015:S:1
	0:10.000015|10.000015:J:1=0,1,2,3

0:20.002021|20.000030:S:2
	0:20.002021|20.002021:N

0:20.839641|20.839641:C:1
	0:20.839641|20.839641:J:2=0,1,2,3

0:25.008078|25.008078:C:2
	0:25.008078|25.008078:N

0:30.000030|30.000030:S:3
	0:30.000030|30.000030:J:3=0,1,2,3

0:40.002021|40.000030:S:4
	0:40.002021|40.002021:J:4=0,1,2,3

0:43.005988|43.005988:C:3
	0:43.005988|43.005988:N

0:143.005988|143.005988:C:4
	0:143.005988|143.005988:N
'''

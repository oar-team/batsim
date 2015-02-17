import struct
import socket
import sys
import os
import json
from random import sample

server_address = '/tmp/bat_socket'

def load_json_workload_profile(filename):
    wkp_file = open(filename)
    wkp = json.load(wkp_file)
    return wkp["jobs"], wkp["nb_res"] 


json_jobs, nb_res = load_json_workload_profile(sys.argv[1])

job_res_req = { j["id"]: j["res"] for j in json_jobs} 

# Make sure the socket does not already exist
try:
    os.unlink(server_address)
except OSError:
    if os.path.exists(server_address):
        raise

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Bind the socket to the port
print >>sys.stderr, 'starting up on %s' % server_address
sock.bind(server_address)

# Listen for incoming connections
sock.listen(1)

jobs_completed = []
jobs_waiting = []

sched_delay = 5.0

while True:
    # Wait for a connection
    print >>sys.stderr, 'waiting for a connection'
    connection, client_address = sock.accept()
    try:
        print >>sys.stderr, 'connection from', client_address

        # Receive the data in small chunks and retransmit it
        while True:
            
            lg_str = connection.recv(4)
            if not lg_str:
                print "connection is closed by batsim core"
                exit(1)
            print 'from client (lg_str): %r' % lg_str
            lg = struct.unpack("i",lg_str)[0]
            print 'size msg to recv %d' % lg
            msg = connection.recv(lg)
            print 'from client: %r' % msg
            sub_msgs = msg.split('|')
            data = sub_msgs[-1].split(":")
            if data[2] != 'T':
                raise Exception("Terminal submessage must be T type")
            time = float(data[1])

            new_jobs_submitted = []
            for i in range(len(sub_msgs)-1):
                data = sub_msgs[i].split(':')
                if data[2] == 'S':
                    new_jobs_submitted.append( int(data[3]) )
                elif data[2] == 'C':
                    jobs_completed.append( int(data[3]) )
                else:
                   raise Exception("Unknow submessage type" + data[2] )  

            time += sched_delay

            if data[2] == 'S':
                msg = "0:" + str(time) + ":J:"
                for job_id in new_jobs_submitted:
                    #
                    # always schedule jobs !
                    # random resources assignment !
                    # 
                    nb_res_req = job_res_req[job_id]
                    res = sample(range(nb_res), nb_res_req)
                    str_res = ",".join([str(r) for r in res])

                    msg += str(job_id) + "=" + str_res + ";"
                    msg = msg[:-1]
            else:
                msg = "0:" + str(time) + ":N"
            print msg
            lg = struct.pack("i",int(len(msg)))
            connection.sendall(lg)
            connection.sendall(msg)
            
    finally:
        # Clean up the connection
        connection.close()
        

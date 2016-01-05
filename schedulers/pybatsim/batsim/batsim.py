#/usr/bin/python3

import json
import struct
import socket

class Batsim(object):

    def __init__(self, json_file, scheduler, server_address = '/tmp/bat_socket', verbose=0):
        self.server_address = server_address
        self.verbose = verbose
        
        self.scheduler = scheduler
        
        #load json file
        self._load_json_workload_profile(json_file)
        
        #open connection
        self._connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        print("[BATSIM]: connecting to %r" % server_address)
        try:
            self._connection.connect(server_address)
            if self.verbose > 1: print('[BATSIM]: connected')
        except socket.error:
            print("[BATSIM]: socket error")
            sys.exit(1)
        
        #initialize some public attributes
        self.last_msg_recv_time = -1
        
        self.scheduler.bs = self
        self.scheduler.onAfterBatsimInit()


    def time(self):
        return self._current_time

    def consume_time(self, t):
        self._current_time += t
        return self._current_time


    def start_job(self, jobid, res):
        self._msgs_to_send.append( ( self.time(), "J:"+str(jobid)+"="+ ",".join([str(i) for i in res]) ) )

    def start_jobs(self, jobids, res):
        msg = "J:"
        for jid in jobids:
            msg += str(jid) + "="
            for r in res[jid]:
                msg += str(r) + ","
            msg = msg[:-1] + ";" # replace last comma by semicolon separtor between jobs
        msg = msg[:-1] # remove last semicolon
        self._msgs_to_send.append( ( self.time(), msg ) )


    def do_next_event(self):
        self._read_bat_msg()

    def start(self):
        while True:
            self.do_next_event()


    def _read_bat_msg(self):
        lg_str = self._connection.recv(4)

        if not lg_str:
            print("[BATSIM]: connection is closed by batsim core")
            exit(1)

        lg = struct.unpack("i",lg_str)[0]
        msg = self._connection.recv(lg).decode()
        if self.verbose > 0: print('[BATSIM]: from batsim : %r' % msg)
        sub_msgs = msg.split('|')
        data = sub_msgs[0].split(":")
        version = int(data[0])
        self.last_msg_recv_time = float(data[1])
        self._current_time = float(data[1])

        if self.verbose > 1: print("[BATSIM]: version: %r  now: %r" % (version, self.time()))
        
        # [ (timestamp, txtDATA), ...]
        self._msgs_to_send = []

        for i in range(1, len(sub_msgs)):
            data = sub_msgs[i].split(':')
            if data[1] == 'R':
                self.scheduler.onJobRejection()
            if data[1] == 'N':
                self.scheduler.onNOP()
            if data[1] == 'S':
                self.scheduler.onJobSubmission(int(data[2]))
            elif data[1] == 'C':
                self.scheduler.onJobCompletion(int(data[2]))
            elif data[1] == 'p':
                opts = data[2].split('=')
                self.scheduler.onMachinePStateChanged(int(opts[0]), int(opts[1]))
            elif data[1] == 'J' or data[1] == 'P':
                raise "Only the server can receive this kind of message"
            else:
                raise Exception("Unknow submessage type " + data[1] )
        
        msg = "0:" + str(self.last_msg_recv_time) + "|"
        if len(self._msgs_to_send) > 0:
            #sort msgs by timestamp
            self._msgs_to_send = sorted(self._msgs_to_send, key=lambda m: m[0])
            for m in self._msgs_to_send:
                msg += str(m[0])+":"+m[1]
        else:
            msg +=  str(self.time()) +":N"

        if self.verbose > 0: print("[BATSIM]:  to  batsim : %r" % msg)
        lg = struct.pack("i",int(len(msg)))
        self._connection.sendall(lg)
        self._connection.sendall(msg.encode())



    def _load_json_workload_profile(self, filename):
        wkp_file = open(filename)
        wkp = json.load(wkp_file)
        
        self.nb_res = wkp["nb_res"]
        self.jobs = {j["id"]: Job(j["id"], j["subtime"], j["walltime"], j["res"], j["profile"]) for j in wkp["jobs"]}
        #TODO: profiles


class Job(object):
    def __init__(self, id, subtime, walltime, res, profile):
        self.id = id
        self.submit_time = subtime
        self.requested_time = walltime
        self.requested_resources = res
        self.profile = profile


class BatsimScheduler(object):
    def onAfterBatsimInit(self):
        #You now have access to self.bs and all other functions
        pass
    def onJobRejection(self):
        raise "not implemented"
    def onNOP(self):
        raise "not implemented"
    def onJobSubmission(self, job):
        raise "not implemented"
    def onJobCompletion(self, job):
        raise "not implemented"
    def onMachinePStateChanged(self, nodeid, pstate):
        raise "not implemented"

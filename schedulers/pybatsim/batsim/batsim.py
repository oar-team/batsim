#/usr/bin/python3

import json
import struct
import socket
import sys



class Batsim(object):

    def __init__(self, json_file, scheduler, validatingmachine=None, server_address = '/tmp/bat_socket', verbose=0):
        self.server_address = server_address
        self.verbose = verbose
        
        if validatingmachine is None:
            self.scheduler = scheduler
        else:
            self.scheduler = validatingmachine(scheduler)
        
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
        self._current_time += float(t)
        return self._current_time

    def wake_me_up_at(self, time):
        self._msgs_to_send.append( ( self.time(), "n:"+str(time) ) )

    def start_jobs_continuous(self, allocs):
        """
        allocs should have the followinf format:
        [ (job, (first res, last res)), (job, (first res, last res)), ...]
        """
        if len(allocs) > 0:
            msg = "J:"
            for (job, (first_res, last_res)) in allocs:
                msg += str(job.id)+ "=" + str(first_res) + "-" + str(last_res)+ ";"
            
            msg = msg[:-1] # remove last semicolon
            self._msgs_to_send.append( ( self.time(), msg ) )
            

    def start_jobs(self, jobs, res):
        if len(jobs) > 0:
            msg = "J:"
            for j in jobs:
                msg += str(j.id) + "="
                for r in res[j.id]:
                    msg += str(r) + ","
                msg = msg[:-1] + ";" # replace last comma by semicolon separtor between jobs
            msg = msg[:-1] # remove last semicolon
            self._msgs_to_send.append( ( self.time(), msg ) )

    def request_consumed_energy(self):
        self._msgs_to_send.append( ( self.time(), "E" ) )
        
    def change_pstates(self, pstates_to_change):
        if len(pstates_to_change) > 0:
            parts = [str(p) + "=" + str(pstates_to_change[p]) for p in pstates_to_change] # create machine=new_pstate string parts
            for part in parts:
                self._msgs_to_send.append( ( self.time(), "P:" + part ) )

    def do_next_event(self):
        self._read_bat_msg()

    def start(self):
        while True:
            self.do_next_event()

    def _time_to_str(self,t):
        return('%.*f' % (6, t))

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
                self.scheduler.onJobSubmission(self.jobs[int(data[2])])
            elif data[1] == 'C':
                j = self.jobs[int(data[2])]
                j.finish_time = float(data[0])
                self.scheduler.onJobCompletion(j)
            elif data[1] == 'p':
                opts = data[2].split('=')
                self.scheduler.onMachinePStateChanged(int(opts[0]), int(opts[1]))
            elif data[1] == 'e':
                consumed_energy = float(data[2])
                self.scheduler.onReportEnergyConsumed(consumed_energy)
            elif data[1] == 'J' or data[1] == 'P' or data[1] == 'E':
                raise "Only the server can receive this kind of message"
            else:
                raise Exception("Unknow submessage type " + data[1] )
        
        msg = "0:" + self._time_to_str(self.last_msg_recv_time) + "|"
        if len(self._msgs_to_send) > 0:
            #sort msgs by timestamp
            self._msgs_to_send = sorted(self._msgs_to_send, key=lambda m: m[0])
            for m in self._msgs_to_send:
                msg += self._time_to_str(m[0])+":"+m[1]+"|"
            msg = msg[:-1]#remove the last "|"
        else:
            msg +=  self._time_to_str(self.time()) +":N"

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
        self.finish_time = None#will be set on completion by batsim
    def __repr__(self):
        return("<Job {0}; sub:{1} res:{2} wtime:{3} prof:{4}>".format(self.id, self.submit_time, self.requested_resources, self.requested_time, self.profile))
    #def __eq__(self, other):
        #return self.id == other.id
    #def __ne__(self, other):
        #return not self.__eq__(other)


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
    def onReportEnergyConsumed(self, consumed_energy):
        raise "not implemented"

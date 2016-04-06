
# -*- coding: utf-8 -*-
import subprocess
import os, time, sys, random
import json


batsim_bin = "../../build/batsim"



def socket_in_use(sock):
    return sock in open('/proc/net/unix').read()

def wait_for_batsim_to_open_connection(sock, timeout=99999999999):
    while timeout > 0 and not socket_in_use(sock):
        time.sleep(0.1)
        timeout -= 0.1
    return timeout > 0


def prepare_batsim_cl(options, sock):
    batsim_cl = [options["batsim_bin"]]

    batsim_cl.append('-e')
    batsim_cl.append(options["output_dir"]+"/"+options["batsim"]["export"])

    if options["batsim"]["energy-plugin"]:
        batsim_cl.append('-p')

    if options["batsim"]["disable-schedule-tracing"]:
        batsim_cl.append('-T')

    batsim_cl.append('-v')
    batsim_cl.append(options["batsim"]["verbosity"])
    
    batsim_cl.append('-s')
    batsim_cl.append(sock)

    batsim_cl.append(options["platform"])
    batsim_cl.append(options["workload"])
    
    return batsim_cl



def prepare_pybatsim_cl(options, sock):
    interpreter = ["python"]
    if 'interpreter' in options["scheduler"]:
        if options["scheduler"]["interpreter"] == "coverage":
            interpreter = ["python", "-m", "coverage", "run", "-a"]
        elif options["scheduler"]["interpreter"] == "pypy":
            interpreter = ["pypy", "-OOO"]
        elif options["scheduler"]["interpreter"] == "profile":
            interpreter = ["python", "-m", "cProfile", "-o", "simul.cprof"]
        else:
            assert False, "Unknwon interpreter"
    
    sched_cl = interpreter
    sched_cl.append("launcher.py")
    
    sched_cl.append(options["scheduler"]["name"])
    
    sched_cl.append(options["workload"])
    
    sched_cl.append("-s")
    sched_cl.append(sock)
    
    sched_cl.append("-o")
    #no need to sanitize the json : each args are given as args in popen
    sched_cl.append(json.dumps(options["scheduler"]["options"]))

    if options["scheduler"]["verbosity"] > 0:
        sched_cl.append('-v')
    if options["scheduler"]["protection"]:
        sched_cl.append('-p')
    return sched_cl




def launch_expe(options, verbose=True):

    options["batsim_bin"] = batsim_bin

    if options["output_dir"] == "SELF":
        options["output_dir"] = os.path.dirname("./"+options["options_file"])
    
    #does an another batsim is in use?
    sock = '/tmp/bat_socket_'+str(random.randint(0, 2147483647))
    while socket_in_use(sock):
        sock = '/tmp/bat_socket_'+str(random.randint(0, 2147483647))

    batsim_cl = prepare_batsim_cl(options, sock)
    batsim_stdout_file = open(options["output_dir"]+"/batsim.stdout", "w")
    batsim_stderr_file = open(options["output_dir"]+"/batsim.stderr", "w")

    sched_cl = prepare_pybatsim_cl(options, sock)
    sched_stdout_file = open(options["output_dir"]+"/sched.stdout", "w")
    sched_stderr_file = open(options["output_dir"]+"/sched.stderr", "w")
    
    print "Starting batsim"
    if verbose: print " ".join(batsim_cl+[">", str(batsim_stdout_file.name), "2>", str(batsim_stderr_file.name)])
    batsim_exec = subprocess.Popen(batsim_cl, stdout=batsim_stdout_file, stderr=batsim_stderr_file, shell=False)

    print "Waiting batsim initialization"
    success = wait_for_batsim_to_open_connection(sock, 20)
    if not success:
        assert False, "error on batsim"
        exit()

    print "Starting scheduler"
    if verbose: print " ".join(sched_cl+[">", str(sched_stdout_file.name), "2>", str(sched_stderr_file.name)])
    sched_exec = subprocess.Popen(sched_cl, stdout=sched_stdout_file, stderr=sched_stderr_file, shell=False)

    print "Wait for the scheduler"
    sched_exec.wait()
    print "Scheduler return code: "+str(sched_exec.returncode)
    
    if sched_exec.returncode >= 2 and batsim_exec.poll() is None:
        print "Terminating batsim"
        batsim_exec.terminate()
    
    print "Wait for batsim"
    batsim_exec.wait()
    print "Batsim return code: "+str(batsim_exec.returncode)
    
    return abs(batsim_exec.returncode) + abs(sched_exec.returncode)
    
    
    

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "usage: launch_expe.py path/to/file.json"
        exit()

    options_file = sys.argv[1]

    options =  json.loads(open(options_file).read())

    options["options_file"] = options_file
    
    exit(launch_expe(options,verbose=True))







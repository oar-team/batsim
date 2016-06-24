#!/usr/bin/python
# encoding: utf-8
'''
Run PyBatsim Sschedulers.

Usage:
    launcher.py <scheduler> <json_file> [-p] [-v] [-s <socket>] [-o <options>]

Options:
    -h --help                                      Show this help message and exit.
    -v --verbose                                   Be verbose.
    -p --protect                                   Protect the scheduler using a validating machine.
    -s --socket=<socket>                           Socket to use [default: /tmp/bat_socket]
    -o --options=<options>                         A Json string to pass to the scheduler [default: {}]
'''


#filler_sched.py ../../workload_profiles/test_workload_profile.json

from batsim.docopt import docopt
import sys, json, string, time
from batsim.batsim import Batsim
from batsim.validatingmachine import ValidatingMachine


from datetime import timedelta


def module_to_class(module):
    """
    transform fooBar to FooBar
    """
    return string.upper(module[0])+module[1:]

def filename_to_module(fn):
    return str(fn).split(".")[0]

def instanciate_scheduler(name, options):
    my_module = name#filename_to_module(my_filename)
    my_class = module_to_class(my_module)

    #load module(or file)
    package = __import__ ('schedulers', fromlist=[my_module])
    if my_module not in package.__dict__:
        print "No such scheduler (module file not found)."
        exit(1)
    if my_class not in package.__dict__[my_module].__dict__:
        print "No such scheduler (class within the module file not found)."
        exit(1)
    #load the class
    scheduler_non_instancied = package.__dict__[my_module].__dict__[my_class]
    scheduler = scheduler_non_instancied(options)
    return scheduler




if __name__ == "__main__":
    #Retrieve arguments
    arguments = docopt(__doc__, version='1.0.0rc2')
    if arguments['--verbose']:
        verbose=999
    else:
        verbose=0

    if arguments['--protect']:
        vm = ValidatingMachine
    else:
        vm = None

    options = json.loads(arguments['--options'])

    scheduler_filename = arguments['<scheduler>']
    json_filename = arguments['<json_file>']
    socket = arguments['--socket']

    print "Starting simulation..."
    print "Workload:", json_filename
    print "Scheduler:", scheduler_filename
    print "Options:", options
    time_start = time.time()
    scheduler = instanciate_scheduler(scheduler_filename, options=options)

    bs = Batsim(json_filename, scheduler, validatingmachine=vm, server_address=socket, verbose=verbose)

    bs.start()
    time_ran = str(timedelta(seconds=time.time()-time_start))
    print "Simulation ran for: "+time_ran
    print "Job received:", bs.nb_jobs_recieved, ", scheduled:", bs.nb_jobs_scheduled, ", in the workload:", bs.nb_jobs_json
    
    if bs.nb_jobs_recieved != bs.nb_jobs_scheduled or bs.nb_jobs_scheduled != bs.nb_jobs_json:
        sys.exit(1)
    sys.exit(0)
    

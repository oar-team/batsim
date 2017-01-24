#!/usr/bin/python2
# encoding: utf-8
'''
Run PyBatsim Sschedulers.

Usage:
    launcher.py <scheduler> [-o <options_string>] [options]

Options:
    -h --help                           Show this help message and exit.
    -v --verbose                        Be verbose.
    -p --protect                        Protect the scheduler using a validating machine.
    -s --socket-endpoint=<endpoint>     Batsim socket endpoint to use [default: tcp://*:5555]
    --redis-hostname=<hostname>         Redis server hostname [default: 127.0.0.1]
    --redis-port=<port>                 Redis server port number [default: 6379]
    --redis-prefix=<prefix>             Redis prefix [default: default]
    -o --options=<options_string>       A Json string to pass to the scheduler [default: {}]
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
        sys.exit(1)
    if my_class not in package.__dict__[my_module].__dict__:
        print "No such scheduler (class within the module file not found)."
        sys.exit(1)
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
    socket_endpoint = arguments['--socket-endpoint']

    # Redis
    redis_hostname = str(arguments['--redis-hostname'])
    redis_port = int(arguments['--redis-port'])
    redis_prefix = str(arguments['--redis-prefix'])

    print "Starting simulation..."
    print "Scheduler:", scheduler_filename
    print "Options:", options
    time_start = time.time()
    scheduler = instanciate_scheduler(scheduler_filename, options=options)

    bs = Batsim(scheduler, validatingmachine=vm, socket_endpoint=socket_endpoint,
                verbose=verbose, redis_hostname=redis_hostname,
                redis_port=redis_port, redis_prefix=redis_prefix)

    bs.start()
    time_ran = str(timedelta(seconds=time.time()-time_start))
    print "Simulation ran for: "+time_ran
    print "Job received:", bs.nb_jobs_received, ", scheduled:", bs.nb_jobs_scheduled

    if bs.nb_jobs_received != bs.nb_jobs_scheduled:
        sys.exit(1)
    sys.exit(0)

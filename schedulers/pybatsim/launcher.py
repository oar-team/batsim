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
import sys, json, string
from batsim.batsim import Batsim
from batsim.validatingmachine import ValidatingMachine




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
        exit()
    if my_class not in package.__dict__[my_module].__dict__:
        print "No such scheduler (class within the module file not found)."
        exit()
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

    scheduler = instanciate_scheduler(scheduler_filename, options=options)

    bs = Batsim(json_filename, scheduler, validatingmachine=vm, server_address=socket, verbose=verbose)

    bs.start()
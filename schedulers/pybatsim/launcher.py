


#filler_sched.py ../../workload_profiles/test_workload_profile.json

import sys
from batsim.batsim import Batsim
from batsim.validatingmachine import ValidatingMachine



scheduler_filename = sys.argv[1]
json_filename = sys.argv[2]


def module_to_class(module):
    """
    transform foo_bar.py to FooBar
    """
    return ''.join(w.title() for w in str.split(module, "_"))

def filename_to_module(fn):
    return str(fn).split(".")[0]

def instanciate_scheduler(name):
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
    scheduler = scheduler_non_instancied()
    return scheduler




scheduler = instanciate_scheduler(scheduler_filename)

#vm = None
vm = ValidatingMachine

bs = Batsim(json_filename, scheduler, validatingmachine=vm, verbose=999)

bs.start()
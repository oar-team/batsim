
import os, json, copy
import glob



schedulers = []




budgets = [0, 2, 0.5]

name_allow = {}
name_allow[(True, False)] = "energyBud"
name_allow[(True, True)] = "reducePC"
name_allow[(False, True)] = "PC"
name_allow[(False, False)] = "SHIT"

name_shut = {}
name_shut[True] = "SHUT"
name_shut[False] = "IDLE"

schedulers += [{
        "name_expe": "easyEnergyBudget_"+str(b)+"_"+name_allow[allow]+"_"+name_shut[shut],
        "name":"easyEnergyBudget",
        "verbosity":10,
        "protection":True,
        "interpreter": "coverage",
        "options": {
            "budget_total": 100*(7-b)*100+30*b*100,
            "budget_start": 10,
            "budget_end": 110,
            "allow_FCFS_jobs_to_use_budget_saved_measured": allow[0],
            "reduce_powercap_to_save_energy": allow[1],
            "monitoring_period":5,
            "power_idle": 30.0,
            "power_compute": 100.0,
            "opportunist_shutdown": shut,
            "pstate_switchon": 0,
            "pstate_switchoff": 1
            }
        } for b in budgets for allow in [(True, False), (True, True), (False, True)] for shut in [True,False]]





#workloads_to_use = glob.glob("traces/*.json")
workloads_to_use = ["../../workload_profiles/stupid.json"]

options = [{
    "platform":"../../platforms/energy_platform_homogeneous_no_net.xml",
    "workload": w,
    "output_dir":"SELF",#where all output files (stdins, stderrs, csvs...) will be outputed.
                        #if set to "SELF" then output on the same dir as this option file.
    "batsim": {
        "export":"out",# The export filename prefix used to generate simulation output
        "energy-plugin": True,#        Enables energy-aware experiments
        "disable-schedule-tracing": True,#remove paje output
        "verbosity": "quiet"  #Sets the Batsim verbosity level. Available values
                                    # are : quiet, network-only, information (default), debug.
        },
    "scheduler": copy.deepcopy(s)
    } for s in schedulers for w in workloads_to_use]





for opt in options:
    opt["scheduler"]["name_expe"] += "_"+os.path.splitext(os.path.basename(opt["workload"]))[0]
    
    new_dir = "tests/"+opt["scheduler"]["name_expe"]
    print new_dir
    try:
        os.mkdir(new_dir)
    except OSError:
        print "ALREADY HERE"
        pass
    #print json.dumps(opt, indent=4)
    open(new_dir+'/expe.json','w').write(json.dumps(opt, indent=4))



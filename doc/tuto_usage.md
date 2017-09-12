This tutorial shows how to execute a Batsim simulation within a development
docker environment.

# Prerequisite
This tutorial supposes that you are on Linux with a working *docker* installation.


# Preparation
In this section, we will set a docker environment. Batsim and the scheduler will
be executed from this environment.

The first step creates the ``battuto`` environment. It is based on the
``batsim_ci`` docker image, which contains all needed dependencies.
``` bash
docker run -ti --name battuto oarteam/batsim_ci bash
```

In the same terminal, running the following commands will install Batsim and
a scheduler in the battuto environment:
``` bash
# get the code
git clone https://gitlab.inria.fr/batsim/batsim.git /root/batsim
cd /root/batsim
git checkout v1.1.0
git submodule update --init

# build and install batsim
mkdir /root/batsim/build
cd /root/batsim/build
cmake .. && make install

# build and install batsched
mkdir /root/batsim/schedulers/batsched/build
cd /root/batsim/schedulers/batsched/build
cmake .. && make install

# create an experiment directory
mkdir /root/battuto
```


# Run the experiment
Both Batsim and the scheduler must be executed within the container.

Batsim can be executed first (in the same terminal used before):
``` bash
batsim -e /root/battuto/out \
       -p /root/batsim/platforms/energy_platform_homogeneous_no_net_128.xml \
       -w /root/batsim/workload_profiles/batsim_paper_workload_example.json \
       --mmax-workload
```

In another terminal, connect to the ``battuto``
``` bash
# Connect to the battuto environment
docker exec -ti battuto bash

# Then, run the scheduler (inside the battuto environment)
batsched -v easy_bf
```

The simulation should start once both processes are executed.
At the end of the simulation, Batsim should display something like this:
```
[master_host:server:(2) 11306.611123] [server/INFO] Simulation is finished!
[11306.611123] [export/INFO] Makespan=11306.611123, scheduling_time=3.099840
```

# Analyze the results
All Batsim results should be in the ``/root/battuto`` directory and start with
``out_`` filenames:
- out_jobs.csv contains information about the jobs execution
- out_machine_states.csv contains information about the machines over time
- out_schedule.csv contains aggregated information about the execution schedule
- out_schedule.trace is the [Pajé](www-id.imag.fr/Logiciels/paje/publications/files/lang-paje.pdf) trace of the execution

Since most output files are in CSV, you can analyze them with any tool you like.

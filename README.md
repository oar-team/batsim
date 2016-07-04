"Batsim
======

Batsim is a Batch Scheduler Simulator. A Batch scheduler AKA Resources and jobs management system (RJMS) is a syteme that manage large scale cluster job placement and scheduling and computation nodes energy policies.

This simulator is made to plug any event based scheduler algorithm to a simulated distributed platform managed by a simulated RJMS. Thus, it permit to compare decission algorithms comming from production and academics worlds.

Here is an overview of how Batsim work compared with real execution.

![Batsim vs. real]

External References
-------------------
* Batsim scientific publication pre-print is available on HAL:
  https://hal.inria.fr/hal-01333471v1

* For a better comprehension of what is Batsim and why it is interesting see the following presentation that was made for JSSPP 2016 IPDPS workshop: 
  [./publications/Batsim\_JSSPP\_2016.pdf]

* Batsim internal documentation can be found [there](http://batsim.gforge.inria.fr/).

Build status
------------

| [Linux (Ubuntu 14.04)][linux-link] |
| :--------------------------------: |
| ![linux-badge]                     |

[linux-badge]: https://travis-ci.org/oar-team/batsim.svg?branch=master "Travis build status"
[linux-link]:  https://travis-ci.org/oar-team/batsim "Travis build status"

Visualisation
-------------

Batsim outpus files can be visualised using external tools:

-   [Evalys] can be used to visualise Gantt chart from the Batsim job.csv files and SWF files
-   [Vite] for the Pajé traces

Tools
-----

Also, some tools can be found in the [tools](./tools) directory:
  - scripts to do conversions between SWF and Batsim formats
  - scripts to setup experiments with Batsim (more details
    [here](./tools/experiments))

Write your own scheduler (or adapt an existing one)
---------------------------------------------------

As Batsim is using a text-based protocol your scheduler have to implement this protocole: For more detail on the protocole, see [protocol description].

A good starting point is Pybatsim which helps you to easily implement your scheduling policy in Python. See [pybatsim folder] for more details.

Installation
------------

### Dependencies

-   SimGrid revision f620ec2586f1596d7 on <git://scm.gforge.inria.fr/simgrid/simgrid.git>
-   rapidjson
-   boost (system, filesystem)
-   C++11 compiler

### First step: generate Makefile via CMake

``` example
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

### Second step: make

``` example
make
```

Usages
------

### Common part for cases (without energy):

In a terminal:

``` example
./build/batsim platforms/small_platform.xml workload_profiles/test_workload_profile.json
```

#### With fake scheduler

In a second terminal:

``` example
python fake_sched_uds.py workload_profiles/test_workload_profile.json
```

#### With filler scheduler

``` example
python filler_sched.py workload_profiles/test_workload_profile.json
```

### Energy experiments

#### Batsim

``` example
./build/batsim -p platforms/energy_platform_homogeneous.xml workload_profiles/test_workload_profile.json
```

#### Scheduler

``` example
python fcfs_sleeper_sched.py workload_profiles/test_workload_profile.json
```

#### With oar core scheduler

In a second terminal:

- First install the required oar-3 libraries.

``` bash
git clone https://github.com/oar-team/oar3.git
cd oar3
sudo python setup.py install
 ```

- Second use **bataar.py** which is the adaptor for OAR3 to interact with BatSim.

 ``` bash
cd oar/kao
./baatar.py ../../batsim/workload_profiles/test_workload_profile.json
```

Refer to [oar3] site or bataar.py -h for more to information.

Display information control
---------------------------

Batsim has its own set of options but it can also handle standard SimGrid ones. The end-of-options delimiter “–” must then be used to use SimGrid options.

``` bash
# Display Batsim options/help
batsim PLATFORM WORKLOAD --help
```

``` bash
# Display SimGrid options/help
batsim PLATFORM WORKLOAD -- --help
```

``` bash
# Quiet run
batsim [batsim_options...] PLATFORM WORKLOAD -q
```

``` bash
# Display network information only
batsim [batsim_options...] PLATFORM WORKLOAD -vnetwork-only
```

``` bash
# Display debug information
batsim [batsim_options...] PLATFORM WORKLOAD -vdebug
```

``` bash
# Generate SimGrid processes' trace (can be useful to visualize what happens)
batsim [batsim_options...] PLATFORM WORKLOAD -t
```

  [Batsim vs. real]: ./doc/batsim_overview.png
  [./publications/Batsim\_JSSPP\_2016.pdf]: ./publications/Batsim_JSSPP_2016.pdf
  [Evalys]: https://github.com/oar-team/evalys
  [Vite]: http://vite.gforge.inria.fr/
  [protocol description]: ./doc/proto_description.md
  [pybatsim folder]: ./schedulers/pybatsim/
  [oar3]: https://github.com/oar-team/oar3


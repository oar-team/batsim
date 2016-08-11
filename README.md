Batsim
======

Batsim is a Batch Scheduler Simulator. A Batch scheduler -- AKA Resources and
Jobs Management System (RJMS) -- is a system that manages resources in
large-scale computing centers, notably by scheduling and placing jobs, and by
setting up energy policies.

Batsim simulates the computing center's behaviour. It is made such that any
event-based scheduling algorithm can be plugged to it. Thus, it permits to
compare decision algorithms coming from production and academics worlds.

Here is an overview of how Batsim works compared to real executions.

![Batsim vs. real]

External References
-------------------
* Batsim scientific publication pre-print is available on HAL:
  https://hal.inria.fr/hal-01333471v1

* For a better understanding of what Batsim is, and why it may be interesting
  for you, give a look at the following presentation, that has been made for
  the JSSPP 2016 IPDPS workshop: [./publications/Batsim\_JSSPP\_2016.pdf]

* Batsim internal documentation can be found
  [there](http://batsim.gforge.inria.fr/).

Build status
------------

| [Linux (Ubuntu 14.04)][travis-history] |
| :------------------------------------: |
| ![][travis-build-st-last-push]         |

[travis-history]: https://travis-ci.org/oar-team/batsim/builds "Travis build history"
[travis-build-st-last-push]: https://travis-ci.org/oar-team/batsim.svg?branch=master "Travis build status (last push)"

Visualisation
-------------

Batsim outpus files can be visualised using external tools:

-   [Evalys] can be used to visualise Gantt chart from the Batsim job.csv files
    and SWF files
-   [Vite] for the Pajé traces

Tools
-----

Also, some tools can be found in the [tools](./tools) directory:
  - scripts to do conversions between SWF and Batsim formats
  - scripts to setup experiments with Batsim (more details
    [here](./tools/experiments))

Write your own scheduler (or adapt an existing one)
---------------------------------------------------

As Batsim is using a text-based protocol, your scheduler has to implement this
protocol: For more detail on the protocol, see [protocol description].

A good starting point is Pybatsim which helps you to easily implement your
scheduling policy in Python. See the [pybatsim folder] for more details.

Try it directly with Docker
----------------------------

You can use the docker container to try it without installation:
``` bash
docker run -ti oarteam/batsim bash
batsim
```

Installation
------------

As Batsim and its dependencies are installed and built by Travis, you can find
an example on how to do it in [this Travis script](.travis.yml).

### Dependencies

Batsim's dependencies are listed below:
-   SimGrid (we recommend the batsim-compatible branch of our fork:
    https://github.com/oar-team/simgrid-batsim/tree/batsim-compatible)
-   RapidJSON (1.02 or greater)
-   Boost 1.48 or greater (system, filesystem)
-   C++11 compiler
-   Redox (and its dependencies: hiredis and libev)

### Building Batsim
Batsim can be built via CMake. An example script is given below:

``` bash
# First step: generate a Makefile via CMake
mkdir build
cd build
cmake .. #-DCMAKE_INSTALL_PREFIX=/usr

# Second step: run make
make -j $(nproc)
sudo make install
```

Batsim Use Cases
------

Simulating with Batsim involves at least two processes:
  - Batsim itself
  - A *decision* process (or simply a *scheduler*)

This section shows Batsim command-line usage and some examples on how to run
simple experiments with Batsim.

### Batsim Usage
Batsim usage can be shown by calling the Batsim program with the ``--help``
option. It should display something like this:
```
Usage: batsim [OPTION...] PLATFORM_FILE WORKLOAD_FILE
A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.

  -e, --export=FILENAME_PREFIX   The export filename prefix used to generate
                             simulation output. Default value: 'out'
  -h, --allow-space-sharing  Allows space sharing: the same resource can
                             compute several jobs at the same time
  -l, --limit-machine-count=M   Allows to limit the number of computing
                             machines to use. If M == -1 (default), all the
                             machines described in PLATFORM_FILE are used (but
                             the master_host). If M >= 1, only the first M
                             machines will be used to comupte jobs.
  -L, --limit-machine-count-by-worload
                             If set, allows to limit the number of computing
                             machines to use. This number is read from the
                             workload file. If both limit-machine-count and
                             limit-machine-count-by-worload are set, the
                             minimum of the two will be used.
  -m, --master-host=NAME     The name of the host in PLATFORM_FILE which will
                             run SimGrid scheduling processes and won't be used
                             to compute tasks. Default value: 'master_host'
  -p, --energy-plugin        Enables energy-aware experiments
  -q, --quiet                Shortcut for --verbosity=quiet
  -s, --socket=FILENAME      Unix Domain Socket filename. Default value:
                             '/tmp/bat_socket'
  -t, --process-tracing      Enables SimGrid process tracing (shortcut for
                             SimGrid options ----cfg=tracing:1
                             --cfg=tracing/msg/process:1)
  -T, --disable-schedule-tracing   If set, the tracing of the schedule is
                             disabled (the output file _schedule.trace will not
                             be exported)
  -v, --verbosity=VERBOSITY_LEVEL
                             Sets the Batsim verbosity level. Available values
                             are : quiet, network-only, information (default),
                             debug.
  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

#### Display information control

Batsim has its own set of options but it can also handle standard SimGrid ones.
The end-of-options delimiter "--" must then be used to use SimGrid options.

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

### Simulating with Batsim

#### Simple Case
This case exhibits Batsim's default behaviour

##### Running Batsim
``` bash
batsim platforms/small_platform.xml \
       workload_profiles/test_workload_profile.json
```

##### Running the (Stupid) Scheduler
``` bash
python2 schedulers/pybatsim/launcher.py fillerSched \
        workload_profiles/test_workload_profile.json
```

#### Specifying Batsim Output and Socket
This case shows how to specify where Batsim should put its output file and which
socket to use, which can be useful to simulate different instances in parallel

##### Preparing the launch
``` bash
mkdir -p /tmp/batsim_output/
```

##### Running Batsim
``` bash
batsim platforms/small_platform.xml \
       workload_profiles/test_workload_profile.json \
       -e /tmp/batsim_output/out \
       -s /tmp/batsim_output/socket
```

##### Running the (Stupid) scheduler
``` bash
python2 schedulers/pybatsim/launcher.py fillerSched \
        workload_profiles/test_workload_profile.json \
        -s /tmp/batsim_output/socket
```

#### Running Energy-Aware experiments
This case shows how to enable energy-aware experiments in Batsim, which allows:
  - to know how much energy the system has been using over time
  - to change the resources' power states (shutdown/DVFS) at simulation time

##### Running Batsim
``` bash
batsim platforms/small_platform.xml \
       workload_profiles/test_workload_profile.json \
       -e /tmp/batsim_output/out \
       -s /tmp/batsim_output/socket \
       -p
```

##### Running the (Stupid) Scheduler
``` bash
python2 schedulers/pybatsim/launcher.py fillerSched \
        workload_profiles/test_workload_profile.json \
        -s /tmp/batsim_output/socket
```

#### Executing Batsim with OAR
This case shows how to execute Batsim with the real RJMS OAR.
Refer to [oar3] site or bataar.py -h for more information.

##### Installing OAR
``` bash
git clone https://github.com/oar-team/oar3.git
cd oar3
sudo python setup.py install
 ```

##### Running Batsim
``` bash
batsim platforms/small_platform.xml \
       workload_profiles/test_workload_profile.json \
       -e /tmp/batsim_output/out \
       -s /tmp/batsim_output/socket
```

##### Running the OAR (Camelot) Scheduler
The scheduler is run via **bataar.py**, which is the OAR3 adaptor to interact
with Batsim.

``` bash
bataar workload_profiles/test_workload_profile.json \
       -s /tmp/batsim_output/socket
```

#### Executing experiments
If you want to run more complex scenarios, giving a look at our
[experiment tools](./tools/experiments) may save you some time!

[Batsim vs. real]: ./doc/batsim_overview.png
[./publications/Batsim\_JSSPP\_2016.pdf]: ./publications/Batsim_JSSPP_2016.pdf
[Evalys]: https://github.com/oar-team/evalys
[Vite]: http://vite.gforge.inria.fr/
[protocol description]: ./doc/proto_description.md
[pybatsim folder]: ./schedulers/pybatsim/
[oar3]: https://github.com/oar-team/oar3

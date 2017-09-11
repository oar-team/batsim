Batsim
======

Batsim is a Batch scheduler simulator.  
A batch scheduler -- AKA Resources and Jobs Management System (RJMS) --
is a system that manages resources in large-scale computing centers,
notably by scheduling and placing jobs, and by setting up energy policies.  
Batsim is open source and distributed under LGPL-3.0 license.
See [copyright](copyright) for more details.

![Batsim overview figure]

Batsim simulates a computing center behavior.  
It is made such that any event-based scheduling algorithm can be plugged to it.  
Thus, it allows to compare decision algorithms coming from production and
academics worlds.

Quick links
-----------
- Please read our [contribution guidelines](CONTRIBUTING.md) if you want to
  contribute to Batsim
- The [changelog](doc/changelog.md) summarizes information about the project
  evolution.
- Tutorials shows how to use Batsim and how it works:
  - The [usage tutorial](doc/tuto_usage.md) explains how to execute a Batsim
    simulation, and how to setup a development docker environment
  - The [time tutorial](doc/tuto_time.md) explains how the time is managed in a
    Batsim simulation, shows essential protocol communications and gives an
    overview of how Batsim works internally
- The [protocol documentation](doc/proto_description.md) defines the protocol
  used between Batsim and the scheduling algorithms

Run batsim example
------------------

**Important note**: It is highly recommended to use Batsim with the provided
container, as up-to-date packages (like boost) that may not be easily available
in your distribution yet.

To test simply test batsim you can directly run it though docker.  First run
batsim in your container for a simple workload:
```bash
# launch a batsim container
docker run -ti --name batsim oarteam/batsim bash

# inside the container
cd /root/batsim
redis-server &
batsim -p platforms/small_platform.xml \
  -w workload_profiles/test_workload_profile.json
```

Then in an *other terminal* execute the scheduler:
```bash
# Run an other bash in the same container
docker exec -ti batsim bash

# inside the container
cd /root/batsim
python schedulers/pybatsim/launcher.py fillerSched
```
External References
-------------------
* Batsim scientific publication pre-print is available on HAL:
  https://hal.inria.fr/hal-01333471v1

* For a better understanding of what Batsim is, and why it may be interesting
  for you, give a look at the following presentation, that has been made for
  the JSSPP 2016 IPDPS workshop: [./publications/Batsim\_JSSPP\_2016.pdf]

* Batsim internal documentation can be found
  [there](http://batsim.gforge.inria.fr/).

Build and code status
------------


| [master][master-link] | [upstream_sg (recent SimGrid)][upstream_sg-link] | [codacy][codacy-link] |
| :-------------------: | :----------------------------------------------: | :-------------------: |
| ![master-badge]       | ![upstream_sg-badge]                             | ![codacy-badge]       |

[master-badge]: https://gitlab.inria.fr/batsim/batsim/badges/master/build.svg "Gitlab CI build status (master branch)"
[master-link]:  https://gitlab.inria.fr/batsim/batsim/pipelines "Gitlab CI build status"
[upstream_sg-badge]: https://gitlab.inria.fr/batsim/batsim/badges/upstream_sg/build.svg "Gitlab CI build status (upstream_sg branch)"
[upstream_sg-link]:  https://gitlab.inria.fr/batsim/batsim/pipelines "Gitlab CI build status"
[codacy-badge]: https://api.codacy.com/project/badge/Grade/e5990f2e9abc4573b13a0b8c9d9e0f08 "Codacy code style"
[codacy-link]: https://www.codacy.com/app/mpoquet/batsim/dashboard

Batsim uses Gitlab CI as its continuous integration system.  
Build status of the different commits can be found
[there](https://gitlab.inria.fr/batsim/batsim/pipelines).  
More information about our CI setup can be found
[there](./doc/continuous_integration.md).

Development environment
-------------------------

If you need to change te code of batsim you can use the docker environment ``oarteam/batsim_ci``
and use the docker volumes to make your batsim version of the code inside the container.
```bash
# launch a batsim container
docker run -ti -v /home/myuser/mybatrepo:/root/batsim --name batsim_dev oarteam/batsim_ci bash

# inside the container
cd /root/batsim
rm -rf build
mkdir build
cd build
cmake ..

# Second step: run make
make -j $(nproc)
make install
make test
```
With this setting you can use your own development tools outside the
container to hack the batsim code and use the container to only to build
and test your your code.

Visualisation
-------------

Batsim output files can be visualised using external tools:

-   [Evalys](http://evalys.readthedocs.io) can be used to visualise Gantt chart from the Batsim job.csv files
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

Schedulers must follow a text-based protocol to communicate with Batsim.  
More details about the protocol can be found in the [protocol description].

You may also base your work on existing Batsim-compatible schedulers:
- C++: [batsched][batsched gitlab]
- D: [datsched][datsched gitlab]
- Perl: [there][perl sched repo] (deprecated)
- Python: [pybatsim][pybatsim gitlab]
- Rust: [there][rust sched repo]

Installation
------------

**Important note**: It is highly recommended to use the method describe in the 
[Development environment](#development-environment) section.

Batsim uses [Kameleon](http://kameleon.imag.fr/index.html) to build controlled
environments. These environments allow us to generate Docker containers, which
are used by [our CI](https://gitlab.inria.fr/batsim/batsim/pipelines) to test
whether Batsim can be built correctly and whether some integration tests pass.

Thus, the most up-to-date information about how to build Batsim dependencies
and Batsim itself can be found in our Kameleon recipes:
  - [batsim_ci.yaml](environments/batsim_ci.yaml), for the dependencies (Debian)
  - [batsim.yaml](environments/batsim.yaml), for Batsim itself (Debian)
  - Please note that [the steps directory](environments/steps/) contain
    subcommands that can be used by the recipes.

However, some information is also written below for the sake of simplicity, but
please note it might be outdated.

### Dependencies

Batsim dependencies are listed below:
-   SimGrid. dev version is recommended (203ec9f99 for example).
    To use SMPI jobs, use commit 587483ebe of
    [mpoquet's fork](https://github.com/mpoquet/simgrid/).
    To use energy, please consider using the Batsim upstream_sg branch and
    SimGrid commit e96681fb8.
-   RapidJSON (1.02 or greater)
-   Boost 1.62 or greater (system, filesystem, regex, locale)
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
----------------

Simulating with Batsim involves at least two processes:
  - Batsim itself
  - A *decision* process (or simply a *scheduler*)

This section shows Batsim command-line usage and some examples on how to run
simple experiments with Batsim.

### Batsim Usage
Batsim usage can be shown by calling the Batsim program with the ``--help``
option. It should display something like this:
```
batsim --help
A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.

Usage:
  batsim -p <platform_file> [-w <workload_file>...]
                            [-W <workflow_file>...]
                            [--WS (<cut_workflow_file> <start_time>)...]
                            [options]
  batsim --help

Input options:
  -p --platform <platform_file>     The SimGrid platform to simulate.
  -w --workload <workload_file>     The workload JSON files to simulate.
  -W --workflow <workflow_file>     The workflow XML files to simulate.
  --WS --workflow-start (<cut_workflow_file> <start_time>)... The workflow XML
                                    files to simulate, with the time at which
                                    they should be started.

Most common options:
  -m, --master-host <name>          The name of the host in <platform_file>
                                    which will be used as the RJMS management
                                    host (thus NOT used to compute jobs)
                                    [default: master_host].
  -E --energy                       Enables the SimGrid energy plugin and
                                    outputs energy-related files.

Execution context options:
  -s, --socket <socket_file>        The Unix Domain Socket filename
                                    [default: /tmp/bat_socket].
  --redis-hostname <redis_host>     The Redis server hostname
                                    [default: 127.0.0.1]
  --redis-port <redis_port>         The Redis server port [default: 6379].

Output options:
  -e, --export <prefix>             The export filename prefix used to generate
                                    simulation output [default: out].
  --enable-sg-process-tracing       Enables SimGrid process tracing
  --disable-schedule-tracing        Disables the Pajé schedule outputting.
  --disable-machine-state-tracing   Disables the machine state outputting.


Platform size limit options:
  --mmax <nb>                       Limits the number of machines to <nb>.
                                    0 means no limit [default: 0].
  --mmax-workload                   If set, limits the number of machines to
                                    the 'nb_res' field of the input workloads.
                                    If several workloads are used, the maximum
                                    of these fields is kept.
Verbosity options:
  -v, --verbosity <verbosity_level> Sets the Batsim verbosity level. Available
                                    values: quiet, network-only, information,
                                    debug [default: information].
  -q, --quiet                       Shortcut for --verbosity quiet

Workflow options:
  --workflow-jobs-limit <job_limit> Limits the number of possible concurrent
                                    jobs for workflows. 0 means no limit
                                    [default: 0].
  --ignore-beyond-last-workflow     Ignores workload jobs that occur after all
                                    workflows have completed.

Other options:
  --allow-time-sharing              Allows time sharing: One resource may
                                    compute several jobs at the same time.
  --batexec                         If set, the jobs in the workloads are
                                    computed one by one, one after the other,
                                    without scheduler nor Redis.
  --pfs-host <pfs_host>             The name of the host, in <platform_file>,
                                    which will be the parallel filesystem target
                                    as data sink/source [default: pfs_host].
  -h --help                         Shows this help.
  --version                         Shows Batsim version.

```

#### Executing complete experiments
If you want to run more complex scenarios, giving a look at our
[experiment tools](./tools/experiments) may save you some time!

[Batsim overview figure]: ./doc/batsim_rjms_overview.png
[./publications/Batsim\_JSSPP\_2016.pdf]: ./publications/Batsim_JSSPP_2016.pdf
[Evalys]: https://github.com/oar-team/evalys
[Vite]: http://vite.gforge.inria.fr/
[protocol description]: ./doc/proto_description.md
[oar3]: https://github.com/oar-team/oar3

[pybatsim gitlab]: https://gitlab.inria.fr/batsim/pybatsim
[batsched gitlab]: https://gitlab.inria.fr/batsim/batsched
[datsched gitlab]: https://gitlab.inria.fr/batsim/datsched
[rust sched repo]: https://gitlab.inria.fr/adfaure/schedulers
[perl sched repo]: https://github.com/fernandodeperto/batch-simulator

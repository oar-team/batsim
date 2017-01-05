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

Run batsim example
------------------

*Important note*: It is highly recommended to use batsim with the provided
container. It use really up-to-date version of some packages (like boost)
that is not available on classic distribution yet.

To test simply test batsim you can directly run it though docker.  First run
batsim in your container for a simple workload:
```
docker run -ti oarteam/batsim bash
cd /root/batsim
batsim -w platforms/small_platform.xml \
  workload_profiles/test_workload_profile.json
```

Then in an other terminal execute the scheduler:
```
docker exec -ti batsim bash
cd /root/batsim
python2 schedulers/pybatsim/launcher.py fillerSched \
  workload_profiles/test_workload_profile.json
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

Build status
------------

[![build status](https://gitlab.inria.fr/batsim/batsim/badges/master/build.svg)]
(https://gitlab.inria.fr/batsim/batsim/commits/master)

Visualisation
-------------

Batsim output files can be visualised using external tools:

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

Installation
------------

Batsim uses [Kameleon](http://kameleon.imag.fr/index.html) to build controlled
environments. These environments allow us to generate Docker containers, which
are used by [our CI](https://gitlab.inria.fr/batsim/batsim/pipelines) to test
whether Batsim can be built correctly and whether some integration tests pass.

Thus, the most up-to-date information about how to build Batsim's dependencies
and Batsim itself can be found in our Kameleon recipes:
  - [batsim_ci.yaml](environments/batsim_ci.yaml), for the dependencies (Debian)
  - [batsim.yaml](environments/batsim.yaml), for Batsim itself (Debian)
  - Please note that [the steps directory](environments/steps/) contain
    subcommands that can be used by the recipes.

However, some information is also written below for simplicity's sake, but
please note it might be outdated.

### Dependencies

Batsim's dependencies are listed below:
-   SimGrid (recommended commit: dccf1b41e9c7b)
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
```bash
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

#### Display information control

Batsim has its own set of options but it can also handle standard SimGrid ones.
The end-of-options delimiter "--" must then be used to use SimGrid options.

``` bash
# Display Batsim options/help
batsim --help
```

``` bash
# Display SimGrid options/help
batsim -- --help
```

#### Executing complete experiments
If you want to run more complex scenarios, giving a look at our
[experiment tools](./tools/experiments) may save you some time!

[Batsim vs. real]: ./doc/batsim_overview.png
[./publications/Batsim\_JSSPP\_2016.pdf]: ./publications/Batsim_JSSPP_2016.pdf
[Evalys]: https://github.com/oar-team/evalys
[Vite]: http://vite.gforge.inria.fr/
[protocol description]: ./doc/proto_description.md
[pybatsim folder]: ./schedulers/pybatsim/
[oar3]: https://github.com/oar-team/oar3

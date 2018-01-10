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

Getting started
---------------

First install batsim and batsched using one of the methods defined the
[install and Run](doc/run_batsim.md) documentation page.

You will also need a platform file that defines the cluster hardware and
topology and a workload that define the set of jobs that will be submitted
to the scheduler and when they will be submitted. Some examples are
available in this repository so you can grab individual files or clone the
whole repo:
```sh
git clone https://gitlab.inria.fr/batsim/batsim.git
cd batsim
```

Now, you can run batsim for a simple workload:
```bash
batsim -p platforms/small_platform.xml -w workload_profiles/test_workload_profile.json
```

Then in an *other terminal* execute the scheduler:
```bash
batsched
```

**Note**: Others worklaods and platforms examples can be found in the
current repository. More sofisticated (and up-to-date) platforms can be
found in the Simgrid repository [https://github.com/simgrid/simgrid]().

External References
-------------------
* Batsim scientific publication pre-print is available on HAL:
  https://hal.inria.fr/hal-01333471v1

* For a better understanding of what Batsim is, and why it may be interesting
  for you, give a look at the following presentation, that has been made for
  the JSSPP 2016 IPDPS workshop: [./publications/Batsim\_JSSPP\_2016.pdf]

* Batsim internal documentation can be found
  [there](http://batsim.gforge.inria.fr/).

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


Build and code status
------------


| [master][master-link] | [upstream_sg (recent SimGrid)][upstream_sg-link] | [codacy][codacy-link] |
| :-------------------: | :----------------------------------------------: | :-------------------: |
| ![master-badge]       | ![upstream_sg-badge]                             | ![codacy-badge]       |

[master-badge]: https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/badges/master/build.svg "Gitlab CI build status (master branch)"
[master-link]:  https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/pipelines "Gitlab CI build status"
[upstream_sg-badge]: https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/badges/upstream_sg/build.svg "Gitlab CI build status (upstream_sg branch)"
[upstream_sg-link]:  https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/pipelines "Gitlab CI build status"
[codacy-badge]: https://api.codacy.com/project/badge/Grade/e5990f2e9abc4573b13a0b8c9d9e0f08 "Codacy code style"
[codacy-link]: https://www.codacy.com/app/mpoquet/batsim/dashboard

Batsim uses Gitlab CI as its continuous integration system.
Build status of the different commits can be found
[CI pipline page][batsim ci]. More information about our CI setup can be
found in the [continuous intefgration](./doc/continuous_integration.md)
documentation.


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

### For users

You can install batsim (and batsched) using one of the methods defined the
[install and Run](doc/run_batsim.md) documentation page.

### For developers

It is highly recommended to use the method describe in the
[Development environment](doc/dev_batsim.md) page to get everything setup and
running: from compilation to tests.

Executing complete experiments
------------------------------

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
[batsim ci]: https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/pipelines

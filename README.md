Batsim
======
[![chat](https://img.shields.io/badge/chat-on%20mattermost-blue.svg)](http://framateam.org/batsim)
[![external issues](https://img.shields.io/badge/external%20issues-github-orange.svg)](https://github.com/oar-team/batsim/issues)
[![internal issues](https://img.shields.io/badge/internal%20issues-inria%20gitlab-orange.svg)](https://gitlab.inria.fr/batsim/batsim/issues)
[![license](https://img.shields.io/badge/license-LGPL%203.0-green.svg)](https://www.gnu.org/licenses/lgpl-3.0.en.html)

[![last release](https://img.shields.io/badge/release-v2.0.0-blue.svg)](https://github.com/oar-team/batsim/releases/tag/v2.0.0)
[![distance from previous release](https://img.shields.io/github/commits-since/oar-team/batsim/v2.0.0.svg)](https://github.com/oar-team/batsim/compare/v2.0.0...HEAD)
[![pipeline status](https://framagit.org/batsim/batsim/badges/master/pipeline.svg)](https://framagit.org/batsim/batsim/pipelines)

[![changelog](https://img.shields.io/badge/doc-changelog-blue.svg)](https://github.com/oar-team/batsim/blob/master/doc/changelog.md)
[![protocol](https://img.shields.io/badge/doc-protocol-blue.svg)](https://github.com/oar-team/batsim/blob/master/doc/proto_description.md)

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

The best way to start to use Batsim, or at least to see how it works, is to have
a look at the (Batsim demo)[./demo/BatsimDemo.ipynb] jupyter notebook.

**Note**: Others workloads and platforms examples can be found in the
current repository. More sophisticated (and more up-to-date) platforms can be
found in the [SimGrid repository](https://github.com/simgrid/simgrid).

External References
-------------------
* Chapters 2 and 3 of Millian Poquet's
  [PhD manuscript](https://mpoquet.github.io/research/phd/manuscript.pdf)
  explain in detail some of Batsim design choices and how Batsim works
  internally. The corresponding
  [defense slides](https://mpoquet.github.io/research/phd/defense_slides.pdf)
  may also interest you.

* Batsim scientific publication pre-print is available on HAL:
  https://hal.inria.fr/hal-01333471v1.
  The corresponding [slides](./publications/Batsim\_JSSPP\_2016.pdf) may
  also interest you for a better understanding of what Batsim is
  and for seeking whether it may be interesting for you.
  These slides have been made for the JSSPP 2016 IPDPS workshop.

* Batsim code documentation can be found
  [there](http://batsim.gforge.inria.fr/batsim/doxygen).

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

Visualisation
-------------

Batsim output files can be visualised using external tools:

-   [Evalys](http://evalys.readthedocs.io) can be used to visualise Gantt chart from the Batsim job.csv files
    and SWF files
-   [Vite] for the Pajé traces

Tools
-----

As Batsim simulation involve multiple processes, they may be tricky to manage.  
Some tools already exist to achieve this goal:
- python tools are located [there](./tools/experiments)
- a more robust and modular approach is conducted
  [there](https://gitlab.inria.fr/batsim/batexpe) and is expected to deprecate
  aforementioned python tools.

You can also find other tools in the [tools](./tools) directory,
for example to conduct convertions between SWF and Batsim workload formats.

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
[experiment tools](./tools/experiments) may save you some time! (May be
deprecated in the future by [batexpe](https://gitlab.inria.fr/batsim/batexpe))

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

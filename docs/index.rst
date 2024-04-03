Batsim
======

.. note::

   Parts of this documentation are still missing.
   Feel free to :ref:`contact_us` if you have any question or remark — or if a TODO is important for you.
   Here are :ref:`contributing`.

.. warning::

   It is **strongly recommended** to check that Batsim behaves as you expect before using it for your experiments: many parameters can strongly influence simulation results, bugs can persist despite our efforts...
   This is all the more important for codes in the Batsim ecosystem that are external to Batsim itself (e.g., batsched_/pybatsim_ scheduling algorithms), which are much less tested than Batsim.
   Please do :ref:`contact_us` in case of doubt.

Batsim is a scientific simulator to analyze batch schedulers.
Batch schedulers — or Resource and Jobs Management Systems, RJMSs — are systems that manage resources in large-scale computing centers, notably by scheduling and placing jobs.

.. image:: img/batsim_rjms_overview.png
   :width: 100 %
   :alt: Batsim overview

- Analyze and compare online scheduling algorithms.
- Sound simulation models thanks to SimGrid_.
- Develop algorithms (in any programming language) without SimGrid knowledge, or to plug existing algorithm implementations to Batsim. Done thanks to a :ref:`protocol` between Batsim and the schedulers
- Several ways to model how jobs should be simulated. Allows multiple levels of realism regarding several phenomena. Highly customizable to your needs.
- Keeping the implementation robust and maintainable is important to us.

The present documentation focuses on Batsim technical aspects.
The most up-to-date scientific description of Batsim is done in `Millian Poquet's PhD thesis`_ (chapters 3 and 4).
There is also the outdated `Batsim initial white paper`_ —
please cite it (`bibtex <https://hal.archives-ouvertes.fr/hal-01333471v1/bibtex>`_) if you use Batsim for your research.

.. toctree::
   :maxdepth: 1
   :caption: Tutorials

   Running your first simulation <tuto-first-simulation/tuto.rst>
   Analyzing Batsim results <tuto-result-analysis/tuto.rst>
   Implementing your scheduling algorithm <tuto-sched-implem/tuto.rst>
   Modelling an application <tuto-app-model/stencil.rst>
   Doing a reproducible experiment <tuto-reproducible-experiment/tuto.rst>

.. toctree::
   :maxdepth: 1
   :caption: User Manual

   Installation <installation.rst>
   Command-line interface <cli.rst>
   Protocol <protocol.rst>
   Time progression <time-dilation.rst>
   Using Redis <redis.rst>
   Using the I/O model <IO.rst>
   Interval set <interval-set.rst>
   Frequently Asked Questions <faq.rst>
   Changelog <changelog.rst>
   Contact us <contact.rst>

.. toctree::
   :maxdepth: 1
   :caption: Simulation inputs

   Platform <input-platform.rst>
   Workload <input-workload.rst>
   Events <input-events.rst>

.. toctree::
   :maxdepth: 1
   :caption: Simulation outputs

   Schedule-centric <output-schedule.rst>
   Job-centric <output-jobs.rst>
   Energy-related <output-energy.rst>

.. toctree::
   :maxdepth: 1
   :caption: Developer Manual

   Contributing <contributing.rst>
   Development environment <setting-up-dev-env.rst>
   How to run tests? <howto-test.rst>
   Continuous Integration <ci.rst>
   Release procedure <release.rst>
   TODO list <todo.rst>

.. _SimGrid: https://simgrid.frama.io/
.. _Batsim initial white paper: https://hal.archives-ouvertes.fr/hal-01333471v1
.. _Millian Poquet's PhD thesis: https://hal.archives-ouvertes.fr/tel-01757245v2
.. _batsched: https://framagit.org/batsim/batsched
.. _pybatsim: https://gitlab.inria.fr/batsim/pybatsim

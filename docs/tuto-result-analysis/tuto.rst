.. _tuto_result_analysis:

Analyzing Batsim results
========================

Prerequisites
-------------
This tutorial assumes that you completed tutorial :ref:`tuto_first_simulation` and have kept the simulation results obtained during the tutorial.

Files overview
--------------
All Batsim output files are textual and are written in the same directory — they actually share their path prefix (see :ref:`cli`).

- ``PREFIX_jobs.csv`` is the main output file. Contains information about the execution of each job (see :ref:`output_jobs`).
- ``PREFIX_schedule.csv`` contains aggregated information about the whole simulation — such as makespan, mean waiting time or total consumed energy (see :ref:`output_schedule`).
- ``PREFIX_schedule.trace`` is a Pajé_ trace of the simulation. Can be visualized with tools such as ViTE_.
- ``PREFIX_machine_states.csv`` is a time series about the platform usage. It stores how many machines are in each state for each time interval. This file is mostly used to have a scalable view of the platform usage over time — this is useful when the number of jobs is big.

Computing some statistics
-------------------------
Most Batsim output files are plain CSV_ and can therefore be loaded in any data analysis framework.

The following script outlines how to do a basic analysis with in R_ without losing sanity thanks to tidyverse_.
The conclusions are of course not amazing on this toy workload.

.. literalinclude:: basic-analysis.R
    :language: R

The script can be executed from the experiment output directory.
It should print some metrics and generate several plots in the current directory.

.. todo::

    We may think of more interesting things to plot while remaining simple.
    This is not easy on this toy workload though...

    Maybe include the plot in this document if it is interesting.

Visualizing Gantt charts
------------------------

| Gantt charts can easily be visualized thanks to the Evalys_ Python library.
| Evalys can take as input a regular SWF workload file or the Batsim ``PREFIX_jobs.csv`` output file to plot the Gantt chart of the jobs with the following script.
| More detailed plots are presented in the examples_ of the Evalys repository.

.. code:: python

    from evalys.jobset import JobSet
    from evalys import visu
    js = JobSet.from_csv("PREFIX_jobs.csv")
    visu.gantt.plot_gantt(js)



.. todo::

    Introduce ViTE_ here and show an output example.


Build your own visualization
----------------------------

.. todo::

    Talk about Evalys_ / custom scripts

.. _Pajé: http://paje.sourceforge.net/
.. _ViTE: http://vite.gforge.inria.fr/
.. _CSV: https://en.wikipedia.org/wiki/Comma-separated_values
.. _R: https://www.r-project.org/
.. _tidyverse: https://www.tidyverse.org/
.. _Evalys: https://github.com/oar-team/evalys
.. _examples: https://github.com/oar-team/evalys#examples
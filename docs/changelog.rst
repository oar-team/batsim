.. _changelog:

Changelog
=========

All notable changes to this project will be documented in this file.
The format is based on `Keep a Changelog`_.
Starting with version `v1.0.0`_, Batsim adheres to `Semantic Versioning`_ and its public API includes the following.

- The Batsim command-line interface.
- The format of the Batsim input files.
- The communication protocol with the decision-making component.

.. todo::

    Define a recommended SimGrid version for each batsim version.

    Link to protocol messages of the rst doc rather than doing ``THIS``.

........................................................................................................................

Unreleased
----------

- `Commits since v2.0.0 <https://github.com/oar-team/batsim/compare/v2.0.0...HEAD>`_
- ``nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsim_dev``

Changed (**breaks protocol**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Removal of the ``NOP`` event.
- ``SUBMIT_PROFILE`` has been renamed ``REGISTER_PROFILE``.
  Trying to register an already existing profile will now fail.
- ``SUBMIT_JOB`` has been renamed ``REGISTER_JOB``.
  Trying to register an already existing job will now fail.
  The possibility to register profiles from within a ``REGISTER_JOB`` event has been discarded.
  Now use ``REGISTER_PROFILE`` then ``REGISTER_JOB``.
- The ``SIMULATION_BEGINS`` event has been changed:

  - The ``resources_data`` array has been split into
    the ``compute_resources`` and ``storage_resources`` arrays.
  - The content of the ``config`` object has been flattened and now contains the following keys:
    ``redis-enabled``, ``redis-hostname``, ``redis-port``, ``redis-prefix``, ``profiles-forwarded-on-submission``, ``dynamic-jobs-enabled`` and ``dynamic-jobs-acknowledged``.
- The ``submission_finished`` ``NOTIFY`` event has been renamed ``registration_finished``.
- The ``continue_submission`` ``NOTIFY`` event has been renamed ``continue_registration``.

Changed (**breaks command-line interface**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Removal of the ``--config-file`` option.
  Everything should now be doable via the Batsim CLI.
- Removal of the ``--enable-sg-process-tracing`` option.
  You can now use ``--sg-cfg`` to do the same.
- ``--batexec`` has been renamed ``--no-sched``.
- ``--allow-time-sharing`` has been split into two options
  ``--enable-compute-sharing`` and ``--disable-storage-sharing``,
  as resource roles have been introduced.

Changed (**breaks workload format**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Profile types of parallel jobs have been renamed:

  - ``msg_par`` into ``parallel``
  - ``msg_par_hg`` into ``parallel_homogeneous``
  - ``msg_par_hg_tot`` into ``parallel_homogeneous_total``
  - ``msg_par_hg_pfs`` into ``parallel_homogeneous_pfs``

Changed (**breaks platform format**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Batsim now uses SimGrid version **TO BE DEFINED** and therefore the
  SimGrid platform version 4.1, which broke things on how to define platforms.
  Please refer to SimGrid documentation for more information on this.

Changed (new dependencies)
~~~~~~~~~~~~~~~~~~~~~~~~~~

- `docopt-cpp`_ and pugixml_ are now external dependencies and no longer provided with Batsim sources.
- New `intervalset` dependency, which replaces the previous `MachineRange` class.
- batexpe_ is now an optional dependency to test batsim.

Added (protocol)
~~~~~~~~~~~~~~~~

- Addition of the ``no_more_static_job_to_submit`` ``NOTIFY`` event,
  which is sent by Batsim when all the jobs described in the static
  workloads/workflows have been submitted.
- Addition of the ``profiles`` object in the ``SIMULATION_BEGINS`` event.
  The key is the workload_id and the value is the list of profiles of that workload.
- Addition of the optional ``storage_mapping`` object in the ``EXECUTE_JOB`` event,
  which allows to define which resource id should be used for a named IO resource.
- Addition of the optional ``additional_io_job`` object in the ``EXECUTE_JOB`` event,
  which allows to add IO movements to a job execution.
  This is done by merging a traditional parallel task (within the allocated hosts that *compute* the job)
  with another parallel task that define IO movements (within the allocated hosts that compute the jobs, but also potentially with IO resources).

Added (platform format)
~~~~~~~~~~~~~~~~~~~~~~~

- Roles can now be specified for the hosts of a platform.
  This is done by setting the ``role`` XML property of a host.
  A default master host can be specified this way by using the ``master`` role value.
  The ``storage`` value is for hosts that describe storage resources ; such hosts are allowed to send and receive bytes but not to compute.
  The ``compute_node`` value (used by default if no role is specified) is for hosts that describe computing resources that can both compute and communicate.

Added (command-line interface)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- New ``--add-role-to-hosts`` option, that allows to add a role to some hosts.
- New ``--sg-cfg`` option, that allows to set SimGrid configuration options.
- New ``--sg-log`` option, that allows to set SimGrid logging options.
- New ``--dump-execution-context`` option,
  that dumps the command execution context on the standard output.
  This allows external tools to understand the execution context of a batsim command without actually parsing it.

Miscellaneous
~~~~~~~~~~~~~
- Various bug fixes.
- Removal of the python experiment scripts that were located in ``${BATROOT}/tools/experiments``,
  as robin_ became the standard tool to execute Batsim experiments.
- Removed git submodules. Please now use schedulers directly from their repositories or from kapack_.
- Batsim now mainly uses the s4u SimGrid interface.
  If you used to set SimGrid configuration/logging options through Batsim CLI,
  the name of such options should therefore have changed.
- Documentation moved to readthedocs.
- The ``workload_profiles`` directory has been renamed ``workloads``.
- New generator for heteregenous platforms (code and documentation in ``platforms/heterogeneous``).
- New demo (in ``demo/``).

........................................................................................................................

v2.0.0
------

- `Commits since v1.4.0 <https://github.com/oar-team/batsim/compare/v1.4.0...v2.0.0>`_
- Release date: 2018-02-20
- ``nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -i batsim-2.0.0``

Changed (**breaks protocol**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- The ``QUERY_REQUEST`` and ``QUERY_REPLY`` messages have been respectively renamed ``QUERY`` and ``ANSWER``.
  This pair of messages is now bidirectional (Batsim can now ask information to the scheduler).
  Redis interactions with this pair of messages is no longer in the protocol (as it has never been implemented).
- When submitting dynamic jobs (``SUBMIT_JOB``), the ``job_id`` and ``id`` fields should now have the same value.
  Furthermore, jobs id are no longer integers but strings: ``my_wload!hello readers`` is now a valid job identifier.
- Removal of the ``job_status`` field from ``JOB_COMPLETED`` messages.
- ``JOB_COMPLETED`` messages should now be sent even for killed jobs.
  In this case, ``JOB_COMPLETED`` should be sent before ``JOB_KILLED``.

Added
~~~~~

- Added the ``--simgrid-version`` command-line option to show which SimGrid is used by Batsim.
- Added the ``--unittest`` command-line option to run unit tests.
  Executed by Batsim’s continuous integration system.
- New ``SET_JOB_METADATA`` protocol message, which allows to set set metadata to jobs.
  Such metadata is written in the ``_jobs.csv`` output file.
- The ``_schedule.csv`` output file now contains a batsim_version field.
- Added the ``estimate_waiting_time`` QUERY from Batsim to the scheduler.
- The ``SIMULATION_BEGINS`` message now contains information about workloads:
  A map from workload identifiers to their filenames.
- Added the ``job_alloc`` field to ``JOB_COMPLETED`` messages,
  which mentions which machines have been allocated to the finished job.

Changed
~~~~~~~

- The ``_jobs.csv`` output file is now written more cleanly.
  The order of the columns within it may have changed.
  Removal of the deprecated ``hacky_job_id`` field.

Fixed
~~~~~

- Numeric sort should now work as expected (this is now tested).
- Power stace tracing now works when the number of machines is big.
- Output buffers now work even if incoming texts are bigger than the buffer.
- The ``QUERY_REQUEST``/``QUERY_REPLY`` messages were not respecting the protocol definition
  (probably never tested since the JSON protocol update).
- Dynamically submitted jobs could not be used right away after being submitted
  (by the following events, or at least the events of the same timestamp). This should now be possible.

........................................................................................................................

v1.4.0
------

- `Commits since v1.3.0 <https://github.com/oar-team/batsim/compare/v1.3.0...v1.4.0>`_
- Release date: 2017-10-07
- ``nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -i batsim-1.4.0``

Added
~~~~~

- New ``SUBMIT_PROFILE`` protocol message that allows the decision process to submit profiles dynamically.
- New ``msg_par_hg_tot`` profile type.
  This is an homogeneous parallel task whose computation and communications amounts are spread over all allocated nodes.
  They can be seen as optimistic moldable tasks.

........................................................................................................................

v1.3.0
------

- `Commits since v1.2.0 <https://github.com/oar-team/batsim/compare/v1.2.0...v1.3.0>`_
- Release date: 2017-09-30

Added
~~~~~

- Jobs walltimes are no longer mandatory.
  The ``walltime`` field of jobs can now be omitted or set to -1.
  Such jobs will never be killed automatically by Batsim.

........................................................................................................................

v1.2.0
------

- `Commits since v1.1.0 <https://github.com/oar-team/batsim/compare/v1.1.0...v1.2.0>`_
- Release date: 2017-09-23

Added
~~~~~

- The job progress is now sent through the protocol when jobs are killed on request.
  This is done via a new ``job_progress`` map in ``JOB_KILLED`` messages,
  which gives this information for all the jobs that have really been killed.
- New job state ``COMPLETED_WALLTIME_REACHED`` (separated from ``COMPLETED_FAILED``).

........................................................................................................................

v1.1.0
------

- `Commits since v1.0.0 <https://github.com/oar-team/batsim/compare/v1.0.0...v1.1.0>`_
- Release date: 2017-09-09

Added
~~~~~

- New job profiles ``SCHEDULER_SEND`` and ``SCHEDULER_RECV`` that communicate with the scheduler.
  New ``send`` and ``recv`` protocol events that correspond to them.
- Jobs now have a return code.
  Can be specified in the ``ret`` field of the jobs in their JSON description.
  Default value is 0 (success).
- New job state: ``COMPLETED_FAILED``.
- New data added to the ``JOB_COMPLETED`` protocol event.
  ``return_code`` indicates whether the job has succeeded.
  The ``FAILED`` status can now be received.

Changed
~~~~~~~

- The ``repeat`` value of sequence (composed) profiles is now optional.
  Default value is 1 (executed once, no repeat).

........................................................................................................................

v1.0.0
------

- `Commits since v0.99 <https://github.com/oar-team/batsim/compare/v0.99...v1.0.0>`_
- Release date: 2017-09-09

Added
~~~~~

- Stated LGPL-3.0 license.
- Code cosmetics standards are now checked by Codacy.
- New PFS host. Associated with a new ``hpst-host`` command-line option.
- New protocol event ``CHANGE_JOB_STATE``.
  It allows the scheduler to change the state of jobs in Batsim in-memory data structures.
- The ``submission_finished`` notification can be cancelled with a ``continue_submission`` notification.
- New data to the ``SIMULATION_BEGINS`` protocol event.
  ``allow_time_sharing`` boolean is now forwarded.
  ``resources_data`` gives information on the resources.
  ``hpst_host`` and ``lcst_host`` give information about the parallel file system.
- New data to the ``JOB_COMPLETED`` protocol event.
  ``job_state`` contains the job state (as stored by Batsim).
  ``kill_reason`` contains why the job has been killed (if relevant).
- New ``continue_submission`` ``NOTIFY`` event,
  which cancels a previous ``submission_finished`` ``NOTIFY`` event.

Modified
~~~~~~~~

-  Improved and renamed parallel file system profiles.
-  Improved code documentation.
-  Improved the python scripts of the tools/ directory.
-  Improved the python scripts of the test/ directory.

Fixed
~~~~~

-  Complex allocation mapping were not handled correctly

........................................................................................................................

v0.99
-----

- Release date: 2017-05-26

Changed
~~~~~~~

-  The protocol is based on ZeroMQ instead of Unix Domain Sockets.
-  The protocol messages are now formatted in JSON (was custom text).

.. _Keep a Changelog: http://keepachangelog.com/en/1.0.0/
.. _Semantic Versioning: http://semver.org/spec/v2.0.0.html
.. _batexpe: https://framagit.org/batsim/batexpe/
.. _robin: https://framagit.org/batsim/batexpe/blob/master/doc/robin.md
.. _kapack: https://github.com/oar-team/kapack/
.. _`docopt-cpp`: https://github.com/docopt/docopt.cpp
.. _pugixml: https://pugixml.org/

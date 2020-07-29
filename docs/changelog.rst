.. _changelog:

Changelog
=========

All notable changes to this project will be documented in this file.
The format is based on `Keep a Changelog`_.
Starting with version `v1.0.0`_, Batsim adheres to `Semantic Versioning`_ and its public API includes the following.

- The Batsim command-line interface.
- The format of the Batsim input files.
- The communication protocol with the decision-making component.

........................................................................................................................

Unreleased
----------

- `Commits since v4.0.0 <https://github.com/oar-team/batsim/compare/v4.0.0...HEAD>`_
- ``nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA batsim-master``

........................................................................................................................

v4.0.0
------

- `Commits since v3.1.0 <https://github.com/oar-team/batsim/compare/v3.1.0...v4.0.0>`_
- ``nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -i batsim-4.0.0``
- Recommended SimGrid release: 3.25.0 (see `SimGrid's framagit releases <https://framagit.org/simgrid/simgrid/releases>`_)

Changed (**breaks some schedulers**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Profiles and jobs are now cleaned from memory over time (instead of at the end of the whole simulation).
  This is done with a reference counting mechanism: When a job or profile is no longer needed **according to what batsim knows**, it is removed from memory.
  This can break schedulers that rely on dynamic profile/job submission, especially when several :ref:`proto_REGISTER_JOB` using the same profile are decided at different simulation times — as the profile can be garbage collected when its first execution finishes.
  The new ``--enable-profile-reuse`` :ref:`cli` option should keep previous behavior.

Removed (**breaks CLI**)
~~~~~~~~~~~~~~~~~~~~~~~~

- As unit tests are now done with gtest_, the ``--unittest`` :ref:`cli` option has been removed.

Added
~~~~~

- Scheduler configuration can be given to Batsim (via ``--sched-cfg`` or ``--sched-cfg-file`` :ref:`cli` options).
  This configuration string is forwarded to the scheduler in the :ref:`proto_SIMULATION_BEGINS` event.
- Basic tests for the external events mechanism.
- Retrieval of the zone properties in the XML platform description.

  - Platform properties declared within SimGrid zones are now retrieved and attached to each Batsim resource.
  - These properties are forwarded to the scheduler via the field ``zone_properties`` or each resource in the ``compute_resources`` and ``storage_resources`` arrays of the :ref:`proto_SIMULATION_BEGINS` event.

Fixed
~~~~~

- Workflows crashed at the beginning and the end of the simulation. This should be fixed, and workflows are now tested under CI.
- Killing jobs should no longer issue memory issues (invalid reads and writes), which caused segmentation fault in corner cases — cf. `issue 37 (inria) <https://gitlab.inria.fr/batsim/batsim/issues/37/>`_.
- Killing sequences of delays should no longer crash with "Internal error" — cf. `issue 108 (inria) <https://gitlab.inria.fr/batsim/batsim/issues/108/>`_.
- SMPI profiles should now be automatically killed when their walltime is reached — cf. `issue 95 (inria) <https://gitlab.inria.fr/batsim/batsim/issues/95/>`_.

Miscellaneous
~~~~~~~~~~~~~

- Various performance improvements.
- The jobs output file is now written over time (was only written on disk at the end of the simulation).
- Batsim no longer uses SimGrid's MSG interface. Everything is done with S4U now.
- Smart pointers are used in most parts of the code (for reference counting memory deallocations).
- Old markdown documentation has been removed.
- Removal of CMake Find functions, pkgconfig is used instead.

........................................................................................................................

v3.1.0
------

- `Commits since v3.0.0 <https://github.com/oar-team/batsim/compare/v3.0.0...v3.1.0>`_
- Release date: 2019-05-26
- ``nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -i batsim-3.1.0``
- Recommended SimGrid release: 3.24.0 (see `SimGrid's framagit releases <https://framagit.org/simgrid/simgrid/releases>`_)

Changed
~~~~~~~

- Batsim now requires that no :ref:`proto_CALL_ME_LATER` are pending to send :ref:`proto_SIMULATION_ENDS`.
- :ref:`input_workload` identifiers are now generated depending on the order of the command-line arguments.
  Previously, they were hashes of the absolute filename of the workload, which was order independent.

Added
~~~~~

- A new :ref:`input_EVENTS` mechanism has been added.

  - For the moment the following external events are supported.

    - ``machine_unavailable``: Some machines are no longer available.
    - ``machine_available``: Some machines are available again.
    - :ref:`events_GENERIC_EVENTS`: User-defined external events that can be forwarded to the scheduler with the option ``--forward-unknown-events``.
  - A new :ref:`proto_NOTIFY` protocol event ``no_more_external_event_to_occur`` has been added to tell the scheduler
    that no more external events coming from Batsim can occur during the simulation.
  - A new command-line option was added: ``--forward-unknown-events`` that forwards unknown external events of the input files to the scheduler (ignored if there were no event inputs).
    The boolean value of this command is forwarded to the scheduler in the ``SIMULATION_BEGINS`` event.

Deprecated
~~~~~~~~~~

- Building via CMake is deprecated. Next Batsim versions may only support Meson_.

Miscellaneous
~~~~~~~~~~~~~

- Removed a build dependency to OpenSSL, which was only used to generate workload identifiers.
- Batsim integration tests are now written with pytest instead of CMake.

........................................................................................................................

v3.0.0
------

- `Commits since v2.0.0 <https://github.com/oar-team/batsim/compare/v2.0.0...v3.0.0>`_
- Release date: 2019-01-15
- ``nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -i batsim-3.0.0``
- Recommended SimGrid commit:
  `97b4fd8e4 <https://framagit.org/simgrid/simgrid/commit/97b4fd8e435a44171d471a245142e6fd0eb992b2>`_

Changed (**breaks protocol**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Removal of the ``NOP`` event.
- ``SUBMIT_PROFILE`` has been renamed :ref:`proto_REGISTER_PROFILE`.
  Trying to register an already existing profile will now fail.
- ``SUBMIT_JOB`` has been renamed :ref:`proto_REGISTER_JOB`.
  Trying to register an already existing job will now fail.
  The possibility to register profiles from within a :ref:`proto_REGISTER_JOB` event has been discarded.
  Now use :ref:`proto_REGISTER_PROFILE` then :ref:`proto_REGISTER_JOB`.
- The :ref:`proto_SIMULATION_BEGINS` event has been changed:

  - The ``resources_data`` array has been split into
    the ``compute_resources`` and ``storage_resources`` arrays.
  - The content of the ``config`` object has been flattened and now contains the following keys:
    ``redis-enabled``, ``redis-hostname``, ``redis-port``, ``redis-prefix``, ``profiles-forwarded-on-submission``, ``dynamic-jobs-enabled`` and ``dynamic-jobs-acknowledged``.
- The ``submission_finished`` :ref:`proto_NOTIFY` event has been renamed ``registration_finished``.
- The ``continue_submission`` :ref:`proto_NOTIFY` event has been renamed ``continue_registration``.

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

- Profile types using parallel tasks have been renamed:

  - ``msg_par`` into ``parallel`` (see :ref:`profile_parallel`)
  - ``msg_par_hg`` into ``parallel_homogeneous`` (see :ref:`profile_parallel_homogeneous`)
  - ``msg_par_hg_tot`` into ``parallel_homogeneous_total`` (see :ref:`profile_parallel_homogeneous_total`)
  - ``msg_par_hg_pfs`` into ``parallel_homogeneous_pfs`` (see :ref:`profile_parallel_homogeneous_pfs`)

Changed (**breaks platform format**)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Batsim now uses SimGrid version 3.21 and therefore the
  SimGrid platform version 4.1, which broke things on how to define platforms.
  Please refer to SimGrid documentation for more information on this.

Changed (jobs/schedule output file format)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **Breaks**: The columns ``requested_number_of_processors`` and ``allocated_processors`` have been respectively renamed ``requested_number_of_resources`` and ``allocated_resources`` in the jobs output file.
- **Breaks**: The order of the columns has changed in the jobs output file.
- The columns ``final_state`` and ``profile`` have been added in the jobs output file.
- The rejected jobs are now present in the jobs and the schedule output files.

Changed (new dependencies)
~~~~~~~~~~~~~~~~~~~~~~~~~~

- `docopt-cpp`_ and pugixml_ are now external dependencies and no longer provided with Batsim sources.
- New intervalset_ dependency, which replaces the previous ``MachineRange`` class.
- batexpe_ is now an optional dependency to test batsim.

Added (protocol)
~~~~~~~~~~~~~~~~

- Addition of the ``no_more_static_job_to_submit`` :ref:`proto_NOTIFY` event,
  which is sent by Batsim when all the jobs described in the static
  workloads/workflows have been submitted.
- Addition of the ``profiles`` object in the :ref:`proto_SIMULATION_BEGINS` event.
  The key is the workload_id and the value is the list of profiles of that workload.
- Addition of the optional ``storage_mapping`` object in the :ref:`proto_EXECUTE_JOB` event,
  which allows to define which resource id should be used for a named IO resource.
- Addition of the optional ``additional_io_job`` object in the :ref:`proto_EXECUTE_JOB` event,
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
  More information in :ref:`platform_host_roles`.

Added (command-line interface)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- New ``--add-role-to-hosts`` option, that allows to add a role to some hosts.
- New ``--sg-cfg`` option, that allows to set SimGrid configuration options.
- New ``--sg-log`` option, that allows to set SimGrid logging options.
- New ``--dump-execution-context`` option,
  that dumps the command execution context on the standard output.
  This allows external tools to understand the execution context of a Batsim command without actually parsing it.

Known issues
~~~~~~~~~~~~

- Killing jobs may now crash in some (corner-case) situations.
  This happens since Batsim upgraded its SimGrid version.
  Tracked on `issue 37 (inria) <https://gitlab.inria.fr/batsim/batsim/issues/37/>`_.
- SMPI profiles only handle relative trace filenames.
  Tracked on `issue 97 (inria) <https://gitlab.inria.fr/batsim/batsim/issues/97/>`_.
- Batsim does not check job size correctly when executed with ``--no-sched``.
  Tracked on `issue 70 (inria) <https://gitlab.inria.fr/batsim/batsim/issues/70/>`_.

Miscellaneous
~~~~~~~~~~~~~
- Various bug fixes.
- Removed the python experiment scripts that were located in ``tools/experiments``,
  as robin_ became the standard tool to execute Batsim experiments.
- Removed git submodules. Please now use schedulers directly from their repositories or from kapack_.
- Removed dependencies to GMP and cppzmq.
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
- Recommended SimGrid commit:
  `587483ebe <https://framagit.org/batsim/simgrid/commit/587483ebe7882eae38ca9aba161fa168834c21e4>`_

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
- The :ref:`proto_SIMULATION_BEGINS` message now contains information about workloads:
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
- Power tracing now works when the number of machines is big.
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
- Recommended SimGrid commit:
  `587483ebe <https://framagit.org/batsim/simgrid/commit/587483ebe7882eae38ca9aba161fa168834c21e4>`_

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
- The ``submission_finished`` notification can be canceled with a ``continue_submission`` notification.
- New data to the :ref:`proto_SIMULATION_BEGINS` protocol event.
  ``allow_time_sharing`` boolean is now forwarded.
  ``resources_data`` gives information on the resources.
  ``hpst_host`` and ``lcst_host`` give information about the parallel file system.
- New data to the ``JOB_COMPLETED`` protocol event.
  ``job_state`` contains the job state (as stored by Batsim).
  ``kill_reason`` contains why the job has been killed (if relevant).
- New ``continue_submission`` :ref:`proto_NOTIFY` event,
  which cancels a previous ``submission_finished`` :ref:`proto_NOTIFY` event.

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
.. _intervalset: https://framagit.org/batsim/intervalset
.. _batexpe: https://framagit.org/batsim/batexpe/
.. _robin: https://framagit.org/batsim/batexpe/blob/master/doc/robin.md
.. _kapack: https://github.com/oar-team/kapack/
.. _`docopt-cpp`: https://github.com/docopt/docopt.cpp
.. _pugixml: https://pugixml.org/
.. _Meson: https://mesonbuild.com/
.. _gtest: https://github.com/google/googletest

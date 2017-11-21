# Changelog
All notable changes to this project will be documented in this file.  
The format is based on [Keep a Changelog][changelog].

Batsim adheres to [Semantic Versioning][semver] from version 1.0.0.  
Batsim's public API includes:
- The Batsim's command-line interface.
- The Batsim input files format.
- The communication protocol with the decision-making component.

[//]: ==========================================================================
## [Unreleased]
### Added
- Added the ``--simgrid-version`` command-line option to show which SimGrid
  is used by Batsim.
- Added the ``--unittest`` command-line option to run unit tests.
  Executed by Batsim's continuous integration system.
- New ``SET_JOB_METADATA`` protocol message, which allows to set
  set metadata to jobs.
  Such metadata is written in the ``_jobs.csv`` output file.
- The ``_schedule.csv`` output file now contains a batsim_version field.

### Changed
- The ``_jobs.csv`` output file is now written more cleanly.  
  The order of the columns within it may have changed.  
  Removal of the deprecated hacky_job_id.

### Fixed
- Numeric sort should now work as expected (this is now tested).
- Power stace tracing now works when the number of machines is big.
- Output buffers now work even if incoming texts are bigger than the buffer.

[//]: ==========================================================================
## [1.4.0] - 2017-10-07
### Added
- New ``SUBMIT_PROFILE`` protocol message that allows the decision process
  to submit profiles dynamically.
- New ``msg_par_hg_tot`` profile type.  
  This is an homogeneous parallel task whose computation and communications
  amounts are spread over all allocated nodes.  
  They can be seen as optimistic moldable tasks.

[//]: ==========================================================================
## [1.3.0] - 2017-09-30
### Added
- Jobs walltimes are no longer mandatory.  
  The ``walltime`` field of jobs can now be omitted or set to -1.  
  Such jobs will never be killed automatically by Batsim.

[//]: ==========================================================================
## [1.2.0] - 2017-09-23
### Added
- The job progress is now sent through the protocol when jobs are killed on
  request.  
  This is done via a new ``job_progress`` map in ``JOB_KILLED``
  messages, which gives this information for all the jobs that have really been
  killed.
- New job state ``COMPLETED_WALLTIME_REACHED``
  (separated from ``COMPLETED_FAILED``).

[//]: ==========================================================================
## [1.1.0] - 2017-09-09
### Added
- New job profiles ``SCHEDULER_SEND`` and ``SCHEDULER_RECV`` that communicate
  with the scheduler.  
  New ``send`` and ``recv`` protocol messages that correspond to them.
- Jobs now have a return code.  
  Can be specified in the ``ret`` field of the jobs in their JSON description.  
  Default value is 0 (success).
- New job state: ``COMPLETED_FAILED``.
- New data in existing protocol messages:
  - ``JOB_COMPLETED``:
    - ``return_code`` indicates whether the job has succeeded
    - The ``FAILED`` status can now be received.

### Changed
- The ``repeat`` value of sequence (composed) profiles is now optional.  
  Default value is 1 (executed once, no repeat).

[//]: ==========================================================================
## [1.0.0] - 2017-09-09
### SimGrid version to use:
- Commit ``587483ebe`` on ``https://github.com/mpoquet/simgrid.git``.  
  Please notice that energy consumption of parallel tasks does not work
  as expected.

### Added
- Stated LGPL-3.0 license.
- Code cosmetics standards are now checked by Codacy.
- New PFS host. Associated with a new ``hpst-host`` command-line option.
- New protocol messages:
  - The ``CHANGE_JOB_STATE`` allows the scheduler to change the state of jobs
    in Batsim in-memory data structures.
  - The ``submission_finished`` notification can be cancelled with a
    ``continue_submission`` notification.
- New data in existing protocol messages:
  - ``SIMULATION_BEGINS``:
    - ``allow_time_sharing`` boolean is now forwarded.
    - ``resources_data`` gives information on the resources.
    - ``hpst_host`` and ``lcst_host give information about the
      parallel file system.
  - ``JOB_COMPLETED``:
    - ``job_state`` contains the job state (as stored by Batsim).
    - ``kill_reason`` contains why the job has been killed (if relevant).
- New tests:
  - ``demo`` corresponds to the documented demonstration.
  - ``energy_minimal`` and ``energy_small`` test the energy plugin in detail.
  - ``kill_multiple`` checks that Batsim can handle multiple kills concerning
    the same jobs.
  - ``fewer_resources`` checks that complex job mapping work
  - ``reject`` tests whether rejection works

### Modified
- Improved and renamed parallel file system profiles.
- Improved code documentation.
- Improved the python scripts of the tools/ directory.
- Improved the python scripts of the test/ directory.

### Fixed
- Complex allocation mapping were not handled correctly

[//]: ==========================================================================
## 0.99 - 2017-05-26
### Changed
- The protocol is based on ZeroMQ instead of Unix Domain Sockets.
- The protocol messages are now JSON-formatted

[//]: ==========================================================================
[changelog]: http://keepachangelog.com/en/1.0.0/
[semver]: http://semver.org/spec/v2.0.0.html

[Unreleased]: https://github.com/oar-team/batsim/compare/v1.4.0...HEAD
[1.4.0]: https://github.com/oar-team/batsim/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/oar-team/batsim/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/oar-team/batsim/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/oar-team/batsim/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/oar-team/batsim/compare/v0.99...v1.0.0

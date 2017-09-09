# Changelog
All notable changes to this project will be documented in this file.  
The format is based on [Keep a Changelog][changelog].

Batsim adheres to [Semantic Versioning][semver] from version 1.0.0.  
Batsim's public API includes:
- The Batsim's command-line interface.
- The Batsim input files format.
- The communication protocol with the decision-making component.

[//]: ==========================================================================
## Unreleased


[//]: ==========================================================================
## 1.0.0 - 2017-09-09
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


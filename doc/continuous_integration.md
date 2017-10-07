Batsim Continuous Integration
=============================

The continuous integration (CI) mechanism used in Batsim is based on Gitlab CI.  
We are currently using the [GRICAD Gitlab server][GRICAD server]
for this purpose.

![gitlab-ci-arch](https://about.gitlab.com/images/ci/arch-1.jpg "Gitlab CI architecture")

Building and Testing Environment
--------------------------------
The CI uses a controlled Docker environment, which has been built with Kameleon
thanks to [this recipe](../environments/batsim_ci.yaml). This allows to:
  - improve separation of concerns
  - avoid installing dependencies within the CI script
  - test whether the Batsim Docker environment works, which is nice for users

Gitlab CI script
----------------
The script can be found [there](../.gitlab-ci.yml). It essentially:
  - builds Batsim with clang, checking that no warning is thrown
  - builds Batsim with gcc, checking that no warning is thrown
  - tests whether Batsim works, running different tests via CMake and
    Batsim experiment tools
  - Checks whether the code is fully documented via Doxygen
  - Deploys the code documentation on
    [this gforge page](http://batsim.gforge.inria.fr/). To do so, some SSH
    key management is done within the script.


Gitlab Project Configuration
----------------------------

Edit your project configuration page ([there for Batsim](https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/edit))
and make sure that ``Pipelines`` are enabled.

Some additional CI related configuration can be done in the CI settings page
([there for Batsim](https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/pipelines/settings)).

Runner Configuration
--------------------

Batsim currently uses Docker runners provided by the GRICAD Gitlab server.  
However, as we previously used our very own machines to host the CI runners,
the rest of this section describes how we managed to do it.

First, install the gitlab-ci-runner on the machine which should execute the
various CI operations. It is probably in your favourite package manager, but
more detailed information can be found on
[the Gitlab CI runner installation manual](https://docs.gitlab.com/runner/install/).

When running a runner for the first time, you have to tell it some information
about the server and the project. This information is given
[there for Batsim](https://gricad-gitlab.univ-grenoble-alpes.fr/batsim/batsim/runners).
``` bash
sudo gitlab-ci-multi-runner register

# Please enter the gitlab-ci coordinator URL (e.g. https://gitlab.com/):
https://gricad-gitlab.univ-grenoble-alpes.fr/

# Please enter the gitlab-ci token for this runner:
[PROJECT-DEPENDENT TOKEN]
```

[GRICAD server]: https://gricad-gitlab.univ-grenoble-alpes.fr

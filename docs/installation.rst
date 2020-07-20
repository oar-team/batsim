.. _installation:

Installation
============
This page presents how to install Batsim and some of its tools.
We recommend `Using Nix`_, but you may prefer `Using Batsim from a Docker container`_ or `Building it yourself`_.

Using Nix
---------
Batsim and its ecosystem are packaged in the kapack_ repository.
These packages use the Nix_ package manager.
We recommend to use Nix as its purity property allows to fully define all the software dependencies of our tools — as well as the versions of each software.
This property is great to produce controlled software environments, as showcased in :ref:`tuto_reproducible_experiment`.

If you already have a working Nix installation, you can skip `Installing Nix`_ and directly go for `Using Batsim from a well-defined Nix environment`_ or `Installing Batsim in your system via Nix`_.

.. note::

    Most package have at least two versions in kapack_, named ``PACKAGE`` and ``PACKAGE-master``. ``PACKAGE`` stands for the latest release of the package, while the ``-master`` version is the latest unstable commit from the main git branch.

.. note::

    Nix commands check if the requested packages — **and ALL their dependencies** — are already available in your system.
    If this is not the case, all the required packages will be built, which may take a lot of time.
    To speed the process up you can enable the use of Batsim's binary cache, so that Nix will download our binaries instead of rebuilding them.

    .. literalinclude:: ./cachix-setup.bash
        :language: bash
        :lines: 3-

Installing Nix
~~~~~~~~~~~~~~

.. note::

    This is unlikely but the procedure to install Nix_ may be outdated.
    Please refer to `Nix installation documentation`_ for up-to-date installation material.

Installing Nix is pretty straightforward.

.. code:: bash

    curl -L https://nixos.org/nix/install | sh

Follow the instructions displayed at the end of the script.
You usually need to source a file to access the Nix commands.

.. code:: bash

    source ~/.nix-profiles/etc/profile.d/nix.sh

.. warning::

   On some distributions like Debian 10, kernel user namespaces are disabled.
   **They should be enabled to make sure Nix works properly.**
   Enable them with the following command:

   .. code:: bash

      sudo echo 1 > /proc/sys/kernel/unprivileged_userns_clone

Using Batsim from a well-defined Nix environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Defining a software environment from which your simulations run is quite straightforward with Nix.
**This is the recommended way to use Batsim.**

For example, the following file defines an en environment from which you can execute batsim and a scheduler implementation.
It uses the last release of our tools.

.. literalinclude:: ./tuto-first-simulation/tuto-env.nix
  :caption: :download:`tuto-env.nix <./tuto-first-simulation/tuto-env.nix>`
  :language: nix

If you want latest commit of the same tools instead, you can define your environment like this:

.. literalinclude:: ./tuto-first-simulation/tuto-env-master.nix
  :caption: :download:`tuto-env-master.nix <./tuto-first-simulation/tuto-env-master.nix>`
  :language: nix

For more information about this usage, please read our tutorial :ref:`tuto_reproducible_experiment`.

Installing Batsim in your system via Nix
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This can be done with ``nix-env --install``.

.. code:: bash

    # Install the Batsim simulator.
    nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA batsim

    # Other packages from the Batsim ecosystem can also be installed this way.
    # For example schedulers.
    nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA batsched
    nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA pybatsim

    # Or interactive visualization tools.
    nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA evalys

    # Or experiment management tools...
    nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA batexpe

Using Batsim from a Docker container
------------------------------------

Batsim and all its runtime dependencies are packaged in the `oarteam/batsim <https://hub.docker.com/r/oarteam/batsim>`_ Docker container, which allows to run batsim without any installation on a Linux host — assuming that Docker is installed.

.. code:: bash

    docker run \
        --net host \
        -u $(id -u):$(id -g) \
        -v $PWD:/data \
        oarteam/batsim:latest \
        -p /data/platf.xml -w /data/wload.json

Here is a quick explanation on the various parameters.

- ``--net host`` enables the use of the host network to communicate with the scheduler.
- ``-u $(id -u):$(id -g)`` enables the generation of output files with your own user permission.
- ``-v $PWD:/data`` shares your local directory so batsim can find input files and write output files.
- ``oarteam/batsim:latest`` is the image to run. ``latest`` is built from master branch's last commit.
- ``-p /data/platf.xml -w /data/wload.json`` are batsim arguments (see :ref:`cli`).

Building it yourself
--------------------
Batsim can be built with the Meson_ (+ Ninja_) build system.
You can also use CMake_ if you prefer but please note that our cmake support is deprecated.

.. warning::

    You first need to install all Batsim dependencies for the following lines to work:

    - Decent clang/gcc (real C++17 support).
    - Decent boost.
    - Recent SimGrid.
    - ZeroMQ.
    - `Redox <https://github.com/mpoquet/redox/tree/install-pkg-config-file>`_ and its dependencies (hiredis, libev).
    - RapidJSON.
    - Pugixml.
    - `Docopt <https://github.com/mpoquet/docopt.cpp/tree/pkgconfig-support>`_.

    **Make sure you install versions of these packages with pkg-config support!**
    The two build systems we use rely on `pkg-config`_ to find dependencies.

    **The dependency list above may be outdated!**
    Please refer to `Batsim packages definition`_ in kapack_ for up-to-date information --- in case of doubt, :ref:`contact_us`.

.. code:: bash

    # Configure the project ; use 'build' as build directory
    meson build # --prefix=/desired/installation/prefix

    # Actually build batsim
    ninja -C build

    # Install batsim
    meson install -C build

.. _kapack: https://github.com/oar-team/nur-kapack/
.. _Nix: https://nixos.org/nix/
.. _Nix installation documentation: https://nixos.org/nix/
.. _CMake: https://cmake.org/
.. _Meson: https://mesonbuild.com/
.. _Ninja: https://ninja-build.org/
.. _pkg-config: https://www.freedesktop.org/wiki/Software/pkg-config/
.. _Batsim packages definition: https://github.com/oar-team/nur-kapack/tree/master/pkgs/batsim
.. _kapack's main file: https://github.com/oar-team/nur-kapack/blob/master/default.nix

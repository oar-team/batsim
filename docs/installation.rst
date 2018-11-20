.. _installation:

Installation
============
This page presents how to install Batsim and some of its tools.
We recommend `Using Nix`_, but you can of course `Build it yourself`_.

Using Nix
---------
Batsim and its ecosystem are packaged in the kapack_ repository.
These packages use the Nix_ package manager.
We recommend to use Nix as its purity property allows to fully define all the software dependencies of our tools — as well as the versions of each software.
This property is great to produce controlled software environments, as showcased in :ref:`tuto_reproducible_experiment`.

If you already have a working Nix installation, you can skip `Installing Nix`_ and directly go for `Installing Batsim in your system via Nix`_.

Installing Nix
~~~~~~~~~~~~~~
Installing Nix is pretty straightforward.

.. code:: bash

    curl https://nixos.org/nix/install | sh

Follow the instructions displayed at the end of the script.
You usually need to source a file to access the Nix commands.

.. code:: bash

    source ~/nix-profiles/etc/profile.d/nix.sh

.. note::

    This is unlikely but the procedure to install Nix_ may be outdated.
    Please refer to `Nix installation documentation`_ for up-to-date installation material.

Installing Batsim in your system via Nix
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This can be done with ``nix-env --install``.

.. note::

    Nix commands check if the requested packages — and **ALL** their dependencies — are already available in your system.
    If this is not the case, all the required packages will be built, which may take a lot of time.
    To speed the process up you can enable the use of Batsim's binary cache, so that Nix will download our binaries instead of rebuilding them.

    .. literalinclude:: ./cachix-setup.bash
        :language: bash
        :lines: 3-

.. code:: bash

    # Install the Batsim simulator.
    nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsim

    # Other packages from the Batsim ecosystem can also be installed this way.
    # For example schedulers.
    nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsched
    nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA pybatsim

    # Or interactive visualization tools.
    nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA evalys

    # Or experiment management tools...
    nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batexpe

.. note::

    Most package have at least two versions in kapack_, named ``PACKAGE`` and ``PACKAGE_dev``. ``PACKAGE`` stands for the latest release of the package, while the ``_dev`` version is the latest unstable commit from the main git branch.

    You can therefore get an upstream Batsim and batsched with the following command. ``nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsim_dev batsched_dev``


Build it yourself
-----------------
Batsim uses the CMake_ build system.
It can therefore be built and installed just like other projects using CMake.

.. note::

    You first need to install all Batsim Dependencies_ for the following lines to work.

.. code:: bash

    # Create a build directory and move into it.
    mkdir build && cd build

    # Generate a Makefile from CMake.
    cmake ..

    # Build Batsim.
    make

    # Install Batsim. Specify -DCMAKE_INSTALL_PREFIX=/desired/prefix to cmake if desired.
    make install

Some options are available via CMake. You can list and edit such options thanks to ccmake_.

Dependencies
~~~~~~~~~~~~

.. warning::

    The following list may be outdated. Please tell us if this is the case.

    An up-to-date list should be available in kapack_.
    The important files there should be the `Batsim package definition`_ and how it is called in `kapack's main file`_ — i.e., with which parameters and which version of each dependency.

As I write these lines, here is the list of Batsim dependencies.

- Decent clang/gcc and CMake.
- Decent boost, GMP (C and C++).
- Recent SimGrid.
- ZeroMQ (C and C++).
- Redox and its dependencies (hiredis, libev).
- RapidJSON.
- Pugixml.
- Docopt.
- OpenSSL.

.. _kapack: https://github.com/oar-team/kapack/
.. _Nix: https://nixos.org/nix/
.. _Nix installation documentation: https://nixos.org/nix/
.. _CMake: https://cmake.org/
.. _ccmake: https://cmake.org/cmake/help/v3.0/manual/ccmake.1.html
.. _Batsim package definition: https://github.com/oar-team/kapack/blob/master/batsim/default.nix
.. _kapack's main file: https://github.com/oar-team/kapack/blob/master/default.nix

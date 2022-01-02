.. _howto_test:

How to run Batsim tests?
========================

.. note::
    Feel free to give a look at our :ref:`ci` script to see how our robots run the tests.

Unit tests
----------

Batsim unit tests are integrated into the Meson_ build system.
Unit tests are also integrated in :download:`default.nix <../default.nix>`.
Batsim can be built with unit tests: ``nix-build -A batsim --arg doUnitTests true`` (assuming that your current working directory is the root of Batsim's git repository).

Unit tests can also be run manually. In this case you can enter a dedicated environment to build batsim thanks to ``nix-shell -A batsim`` — or define your own environment. This enables incremental builds and can be convenient while your are editing Batsim's source code.

#. Tell Meson to compile unit tests by setting the ``-Ddo_unit_tests`` option when *configuring* your Meson build: ``meson build -Ddo_unit_tests=true``.
#. Compile as usual (with Ninja_): ``ninja -C build``. This should generate an executable file ``batunittest`` in charge of running unit tests.
#. Run ``batunittest`` manually (``./build/batunittest``) or via Meson (``meson test -C build``).

Integration tests
-----------------

Most Batsim tests are integration tests that do the following.

#. Execute a simulation instance on well-defined input and with a well-defined scheduler (including scheduler parameters).
#. Make sure that the execution went well (no infinite loop, no Batsim or scheduler crash...).
#. (Read simulation output files and check that they match some properties.)

Integration tests are written in Python with pytest_ in the ``test`` directory (from the root directory of Batsim's git repository).
As in :ref:`tuto_first_simulation`, most of these tests use batsched_ and robin_.

The simplest way to run the integration tests on the current local sources of Batsim is to run
``nix-build -A integration_tests`` from the root directory of Batsim's git repository.
This will do the following.

#. (Recompile Batsim from local files if needed.)
#. Get into an environment with the local Batsim and the various test dependencies.
#. Run all integration tests.
#. Generate an HTML test report.
#. Put results in the `nix store` — accessible from a ``result`` symbolic link.

Results and logs should be convenient to explore from the HTML report.
In other words: ``firefox ./result/pytest_report.html``.

Alternatively, you can enter a shell where all the dependencies are available (``nix-shell -A integration_tests``) and call pytest_ manually from there.
Please note that with this approach, you must reenter the shell whenever you modify Batsim's source code (as it needs to be compiled again).
When you are working on a specific test, it can be useful to only run this test while compiling Batsim if needed before running it. More advanced ``nix-shell`` commands such as ``nix-shell -A integration_tests --command "pytest test/test_nosched.py"`` can be very useful in this case.

Other tests
-----------

The generation of Batsim's Doxygen_ documentation should not issue any warning.
This can be checked manually or just by running
``nix-build -A doxydoc`` from Batsim's repository root directory.

.. _batsched: https://framagit.org/batsim/batsched
.. _Doxygen: http://www.doxygen.nl/
.. _pytest: https://docs.pytest.org/en/latest/
.. _robin: https://framagit.org/batsim/batexpe/
.. _Meson: https://mesonbuild.com/
.. _Ninja: https://ninja-build.org/

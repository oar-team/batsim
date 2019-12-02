.. _howto_test:

How to run Batsim tests?
========================

.. note::
    Feel free to give a look our :ref:`ci` script to see how our robots run the tests.

Unit tests
----------

Batsim unit tests are integrated into the Meson_ build system.
Unit tests are also integrated in :download:`release.nix <../release.nix>`.
Batsim can be built with unit tests: ``nix-build ./release.nix -A batsim --arg doUnitTests true``.

Otherwise, unit tests can be run manually.

#. Tell Meson to compile unit tests: Give the ``-Ddo_unit_tests=true`` option when *configuring* your Meson build.
#. Compile as usual (with Ninja_). This should generate an executable file ``batunittest`` in charge of running unit tests.
#. Run ``batunittest`` manually (``./build/batunittest``) or via Meson (``meson test -C build``).

Integration tests
-----------------

Most Batsim tests are integration tests that do the following.

#. Execute a simulation instance on well-defined input and with a well-defined scheduler (including scheduler parameters).
#. Make sure that the execution went well (no infinite loop, no Batsim or scheduler crash...).
#. (Check that simulation results match some properties.)

Integration tests are written in Python with pytest_ in the ``test`` directory (from Batsim's repository root directory).
As in :ref:`tuto_first_simulation`, most of these tests use batsched_ and robin_.

The simplest way to run the integration tests on the current local sources of Batsim is to run
``nix-build ./release.nix -A integration_tests`` from Batsim's repository root directory.
This will do the following.

#. (Recompile Batsim from local files if needed.)
#. Get into an environment with the local Batsim and the various test dependencies.
#. Run all integration tests.
#. Generate an HTML report.
#. Put results in the `nix store` — accessible from a ``result`` symbolic link.

Results and logs should be convenient to explore from the HTML report.
In other words: ``firefox ./result/pytest_report.html``.

Alternatively, you can enter a `nix-shell` where all the dependencies are available and call pytest_ manually from there.
The drawback of this approach if that the used Batsim might not be up-to-date with your local sources.
`nix-shell`'s ``--command`` can be handy in such cases.
For example, the following commands runs a subset of the integration tests (recompiling Batsim if needed):
``nix-shell ./release.nix -A integration_tests --command "pytest test/test_nosched.py"``.

Other tests
-----------

The generation of Batsim's Doxygen_ documentation should not issue any warning.
This can be checked manually or just by running
``nix-build ./release.nix -A doxydoc`` from Batsim's repository root directory.

.. _batsched: https://framagit.org/batsim/batsched
.. _Doxygen: http://www.doxygen.nl/
.. _pytest: https://docs.pytest.org/en/latest/
.. _robin: https://framagit.org/batsim/batexpe/
.. _Meson: https://mesonbuild.com/
.. _Ninja: https://ninja-build.org/

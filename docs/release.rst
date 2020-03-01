.. _release:

Release procedure
=================

.. note::

    The following notations are used on this page.

    - X.Y.Z stands for the new release.
    - A.B.C stands for the previous release.


Any release
-----------

#. Determine what version number should be released according to `Semantic Versioning`_.
   Major version should be changed whenever an incompatible change occurs on the CLI or the protocol.
#. Update the :ref:`changelog`.
   All modifications since last release that may impact users should be given.
   ``git diff vA.B.C [--stat]`` can be helpful here.
#. Bump version wherever needed. ``git grep 'A.B.C'`` can be helpful here.
#. Tag new version with annotations: ``git tag -a vX.Y.Z``.
   Put the changelog of the new release in the long description.
#. Push the tag on the :ref:`ci` 's repo and make sure it passes.
   Pushing a tag can be done via ``git push REMOTE vX.Y.Z``.
   If the tag is not validated by :ref:`ci`, delete the tag
   (``git tag --delete vX.Y.Z && git push --delete REMOTE vX.Y.Z``),
   fix the issue and start again.
#. Push the tag on all remotes.

Main release (not for pre-releases)
-----------------------------------

#. Do all steps of `Any release`_.
#. Update the `releases` branch. **Do not resolve the merge as a fast-forward!**
   This should be done via ``git checkout releases && git merge --no-ff vX.Y.Z``.
   The goal is that the `releases` branch only contains released versions
   (``git log --first-parent --oneline releases``).
#. Push the `releases` branch on all remotes (``git push REMOTE releases``).
#. Create a new Batsim package in kapack_.

   - The package name should follow Nix standard nomenclature (``nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -i batsim-X.Y.Z``).
   - The default ``batsim`` should point to the new package (``nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -iA batsim``).
   - **The previous Batsim package should remain accessible** (``nix-env -f https://github.com/oar-team/nur-kapack/archive/master.tar.gz -i batsim-A.B.C``).

.. _Semantic Versioning: http://semver.org/spec/v2.0.0.html
.. _kapack: https://github.com/oar-team/nur-kapack/

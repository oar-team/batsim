.. _contributing:

Contributing guidelines
=======================

- Please feel free to :ref:`contact_us` in case of doubt.
- You found a bug or desire a feature? `Open an issue`_.
- You want to improve Batsim's code or documentation? `Open a merge/pull request`_.

Open an issue
-------------

This is mainly done on `Inria Gitlab issues`_,
but also on `GitHub issues`_ for external users.

If you found a bug, please provide a `minimal working example`_ so we can reproduce it.
Please also tell us about your software environment.
Giving versions for Batsim, SimGrid and the scheduler you use is especially important.


Open a merge/pull request
-------------------------

This is mainly done on `Inria Gitlab MRs`_,
but also on `GitHub PRs`_ for external users.

Do not hesitate to :ref:`contact_us` or to `Open an issue`_ before implementing
your work.
This can save you some time,
especially if the contribution does not match `Batsim objectives`_.

If you plan to do several unrelated improvements,
please do several merge/pull requests.
Furthermore, please respect the following git usage.

- Create a dedicated git branch for your bugfix or feature implementation.
- Base your branch on Batsim's up-to-date master branch.
- Do not put unrelated commits in the branch.
- Do not put merge commits in the branch.

.. note::

    If you already have modifications and do not know how to make them fit
    this usage, please read `Atlassian's git tutorial on rebasing`_.

Batsim objectives
-----------------

- Reduce maintenance cost. As we lack development manpower, this is very important.
- `Keep it simple, stupid`_.
  We try to avoid very specific code in Batsim itself.
  Ideally, the features should be modular enough to allow a wide use case variety.
  For example, the :ref:`protocol` should propose a set of basic operations that
  can be composed to achieve what you want,
  rather than a dedicated operation that does exactly what you want.
- Performance should be kept in mind, even if this is not the main concern.

.. _Inria Gitlab issues: https://gitlab.inria.fr/batsim/batsim/issues
.. _Inria Gitlab MRs: https://gitlab.inria.fr/batsim/batsim/merge_requests
.. _GitHub issues: https://github.com/oar-team/batsim/issues
.. _GitHub PRs: https://github.com/oar-team/batsim/pulls
.. _minimal working example: https://en.wikipedia.org/wiki/Minimal_working_example
.. _Atlassian's git tutorial on rebasing: https://www.atlassian.com/git/tutorials/merging-vs-rebasing
.. _Keep it simple, stupid: https://en.wikipedia.org/wiki/KISS_principle

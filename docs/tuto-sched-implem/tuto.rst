.. _tuto_sched_implem:

Implementing your scheduling algorithm
======================================

Studying the scheduling algorithm of your dreams in Batsim involves different things.

1. Implementing the scheduling algorithm itself.
2. Allowing the algorithm to communicate with Batsim through the :ref:`protocol`.
3. Analyzing the algorithm behavior.

This tutorial focuses on the first two parts — as the third is done in :ref:`tuto_result_analysis`.

Gaining insight on the Batsim protocol
--------------------------------------

The Batsim :ref:`protocol` defines the interactions between Batsim and the scheduling algorithm.
**Reading the protocol description is strongly recommended**,
as it defines the following.

- The events the algorithm can react to.
- The actions that the algorithm can take.

Add your algorithm in a project already compatible with Batsim
--------------------------------------------------------------

This is the easiest way to implement your algorithm.
Several projects gather collections of scheduling algorithms and implement the
Batsim :ref:`protocol` to communicate with the Batsim simulator.


+----------------+----------+----------------------------------------------------------------------+----------------------------------------------------------+
| Project        | Language | Pros                                                                 | Cons                                                     |
+================+==========+======================================================================+==========================================================+
| batsched_      | C++      | Kept **up-to-date** with most Batsim changes (used by Batsim tests). | C++: Error-prone, painful to write and maintain.         |
+----------------+----------+----------------------------------------------------------------------+----------------------------------------------------------+
| pybatsim_      | Python   | Python: Great for **prototyping**.                                   | Code lacks consistency. Python: Painful for robustness?  |
+----------------+----------+----------------------------------------------------------------------+----------------------------------------------------------+
| datsched_      | D        | D: Great **(dev productivity, code maintenability)** tradeoff.       | Uses **old protocol** (1.4.0). Just a prototype for now. |
+----------------+----------+----------------------------------------------------------------------+----------------------------------------------------------+
| batsim_rust_   | Rust     | Rust: **Safe and fast**.                                             | Uses **old protocol** (1.4.0).                           |
+----------------+----------+----------------------------------------------------------------------+----------------------------------------------------------+

.. todo::
    Hacking guidelines are not written yet about these projects.

    Do not hesitate to :ref:`contact_us` about it.


Create a new project from scratch
---------------------------------

This can be needed if you want to use another programming language,
or it may be justified if the design decisions of existing implementations does
not match what you want to do.

In this case, your best bet is to refer to the :ref:`protocol` and to
have a look at existing implementations.

.. _batsched: https://framagit.org/batsim/batsched
.. _datsched: https://gitlab.inria.fr/batsim/datsched
.. _pybatsim: https://gitlab.inria.fr/batsim/pybatsim
.. _batsim_rust: https://gitlab.inria.fr/adfaure/bat-rust

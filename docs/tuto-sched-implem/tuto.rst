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
as it defines the following:

- The events the algorithm can react to.
- The actions that the algorithm can take.

Interfacing with Batsim
-----------------------

There are two ways to implement an External Decision Component (EDC) and connect to Batsim:

- *EDC as a library*: provide a C/C++ library dynamically loaded by Batsim's process using the `-l` or `-L` :ref:`cli` option.
- *EDC as a process*: execute a dedicated process that communicates with Batsim's process via a socket, using the `-s` or `-S` :ref:`cli` option.


EDC as a library
----------------

Examples of EDC as libraries are found in `test/edc-lib/`.
An EDC as a library must define the three following functions:

- `uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size);`
- `uint8_t batsim_edc_deinit();`
- `uint8_t batsim_edc_take_decisions(const uint8_t *what_happened, uint32_t what_happened_size, uint8_t **decisions, uint32_t *decisions_size);`


EDC as a process
----------------

Several projects provide an API in different programming languages and implement the Batsim :ref:`protocol` to communicate with Batsim's process.

.. todo::
    Update me. Pybatsim is probably the only maintained API aligned with the new batprotocol and Batsim v5.0.

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

.. _output_real_exec_info:

Real Execution Information
==========================

Aggregated information about the real execution is exported as *prefix* + ``real_exec_info.json`` (the prefix is `out/` by default, see :ref:`cli`).
The file is formatted as a JSON object of a single depth which contains the following fields in lexicographic order of the fields name.

- ``memory_VmHWM_kB``: The peak number of pages really used in memory (read from `/proc/self/status`).
- ``memory_VmPeak_kB``: The peak number of pages in the process address space (read from `/proc/self/status`).
- ``time_in_edc_seconds``: The (real world) time (in seconds) spent in the EDC (and in the network).
- ``time_in_simu_seconds``: The (real world) duration (in seconds) of the whole simulation.

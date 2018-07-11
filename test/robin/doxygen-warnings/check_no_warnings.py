#!/usr/bin/env python3
"""Run doxygen on batsim code, and check that there is no warning."""
import subprocess
import sys

# The path from where doxygen should be executed
doxygen_path = sys.argv[1]

# Run doxygen
proc = subprocess.run("doxygen", cwd=doxygen_path, stdout=subprocess.PIPE)
if proc.returncode != 0:
    print('doxygen did not return 0. Aborting.')
    sys.exit(1)

# Read doxygen logs
logs = open('{p}/doxygen_warnings.log'.format(p=doxygen_path)).read()
if len(logs) == 0:
    print('No doxygen warnings! :)')
    sys.exit(0)
else:
    print("{}\nThere are doxygen warnings :(. Aborting.".format(logs))
    sys.exit(1)

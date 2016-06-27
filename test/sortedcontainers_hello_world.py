#!/usr/bin/python2

#####################
# Debug information #
#####################

import random
import sys

print(sys.version)
help('modules')
help('modules sortedcontainers')

###############
# Real script #
###############

from sortedcontainers import SortedSet

# SortedSet creation
s = SortedSet()

# Filling it with random values
nb_insert = 20
assert(nb_insert >= 1)
for n in range(nb_insert):
    s.add(random.randint(10,99))

# Display
print '[' + ', '.join([str(v) for v in s]) + ']'

# Check that sorting works
for i in range(len(s)-1):
    assert(s[i] <= s[i+1])

# Check that the set length is correct
assert(len(s) >= 1 and len(s) <= nb_insert)

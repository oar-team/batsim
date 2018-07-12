#!/usr/bin/env python3

"""Tools common types and functions."""
from sortedcontainers import SortedSet


def take_first_resources(available_res, nb_res_to_take):
    """Take the first nb_res_to_take resources into available_res."""
    if nb_res_to_take > len(available_res):
        raise Exception('Invalid schedule. Want to use {} resources whereas '
                        'only {} are available({})'.format(nb_res_to_take,
                                                           len(available_res),
                                                           available_res))
    allocation = SortedSet()
    l = list(available_res)

    for i in range(nb_res_to_take):
        allocation.add(l[i])

    available_res.difference_update(allocation)

    if len(available_res) > 0:
        min_available_res = min(available_res)
        for res in allocation:
            if res >= min_available_res:
                raise Exception('Invalid sort')

    return (available_res, allocation)

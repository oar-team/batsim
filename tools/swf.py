#!/usr/bin/env python3

"""SWF types and functions."""

from enum import Enum, unique


@unique
class SwfField(Enum):
    """Maps SWF columns and their meaning."""

    JOB_ID = 1
    SUBMIT_TIME = 2
    WAIT_TIME = 3
    RUN_TIME = 4
    ALLOCATED_PROCESSOR_COUNT = 5
    AVERAGE_CPU_TIME_USED = 6
    USED_MEMORY = 7
    REQUESTED_NUMBER_OF_PROCESSORS = 8
    REQUESTED_TIME = 9
    REQUESTED_MEMORY = 10
    STATUS = 11
    USER_ID = 12
    GROUP_ID = 13
    APPLICATION_ID = 14
    QUEUD_ID = 15
    PARTITION_ID = 16
    PRECEDING_JOB_ID = 17
    THINK_TIME_FROM_PRECEDING_JOB = 18

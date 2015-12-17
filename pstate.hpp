#pragma once

struct Machine;

enum class PStateType
{
    COMPUTATION_PSTATE,
    SLEEP_PSTATE,
    TRANSITION_VIRTUAL_PSTATE
};

struct SleepPState
{
    int sleep_pstate;
    int switch_on_virtual_pstate;
    int switch_off_virtual_pstate;
};

int switch_on_machine_process(int argc, char * argv[]);
int switch_off_machine_process(int argc, char * argv[]);


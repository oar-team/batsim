#pragma once

#include <map>
#include <list>
#include <string>

#include "machine_range.hpp"

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

class CurrentSwitches
{
public:
    struct Switch
    {
        int target_pstate;
        MachineRange switching_machines;
        std::string reply_message_content;
    };

public:
    void add_switch(const MachineRange & machines, int target_pstate);
    bool mark_switch_as_done(int machine_id, int target_pstate, std::string & reply_message_content);

private:
    std::map<int, std::list<Switch *>> _switches;
};


int switch_on_machine_process(int argc, char * argv[]);
int switch_off_machine_process(int argc, char * argv[]);


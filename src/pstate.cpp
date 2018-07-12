/**
 * @file pstate.cpp
 * @brief Contains structures, classes and functions related to power states
 */

#include "pstate.hpp"

#include <simgrid/msg.h>

#include "ipp.hpp"
#include "context.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(pstate, "pstate"); //!< Logging

int switch_on_machine_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    SwitchPStateProcessArguments * args = (SwitchPStateProcessArguments *) MSG_process_get_data(MSG_process_self());

    int machineID = args->machine_id;
    int pstate = args->new_pstate;

    xbt_assert(args->context->machines.exists(machineID));
    Machine * machine = args->context->machines[machineID];

    xbt_assert(machine->host == MSG_process_get_host(MSG_process_self()));
    xbt_assert(machine->state == MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING);
    xbt_assert(machine->jobs_being_computed.empty());
    xbt_assert(machine->has_pstate(pstate));
    xbt_assert(machine->pstates[pstate] == PStateType::COMPUTATION_PSTATE);

    int current_pstate = MSG_host_get_pstate(machine->host);
    int on_ps = machine->sleep_pstates[current_pstate]->switch_on_virtual_pstate;

    XBT_INFO("Switching machine %d ('%s') ON. Passing in virtual pstate %d to do so", machine->id,
             machine->name.c_str(), on_ps);
    MSG_host_set_pstate(machine->host, on_ps);
    //args->context->pstate_tracer.add_pstate_change(MSG_get_clock(), machine->id, on_ps);

    msg_host_t host_list[1] = {machine->host};
//    double flop_amount[1] = {1};
//    double bytes_amount[1] = {0};
    double * flop_amount = xbt_new(double, 1);
    double * bytes_amount = xbt_new(double, 1);
    flop_amount[0] = 1;
    bytes_amount[0] = 0;

    msg_task_t bootup = MSG_parallel_task_create("switch ON", 1, host_list, flop_amount, bytes_amount, NULL);
    XBT_INFO("Computing 1 flop to simulate time & energy cost of switch ON");
    MSG_task_execute(bootup);
    MSG_task_destroy(bootup);

    XBT_INFO("1 flop has been computed. Switching machine %d ('%s') to computing pstate %d",
             machine->id, machine->name.c_str(), pstate);
    MSG_host_set_pstate(machine->host, pstate);
    //args->context->pstate_tracer.add_pstate_change(MSG_get_clock(), machine->id, pstate);

    machine->update_machine_state(MachineState::IDLE);

    SwitchMessage * msg = new SwitchMessage;
    msg->machine_id = args->machine_id;
    msg->new_pstate = args->new_pstate;
    send_message("server", IPMessageType::SWITCHED_ON, (void *) msg);

    delete args;
    return 0;
}

int switch_off_machine_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    SwitchPStateProcessArguments * args = (SwitchPStateProcessArguments *) MSG_process_get_data(MSG_process_self());

    int machineID = args->machine_id;
    int pstate = args->new_pstate;

    xbt_assert(args->context->machines.exists(machineID));
    Machine * machine = args->context->machines[machineID];

    xbt_assert(machine->host == MSG_process_get_host(MSG_process_self()));
    xbt_assert(machine->state == MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING);
    xbt_assert(machine->jobs_being_computed.empty());
    xbt_assert(machine->has_pstate(pstate));
    xbt_assert(machine->pstates[pstate] == PStateType::SLEEP_PSTATE);

    int off_ps = machine->sleep_pstates[pstate]->switch_off_virtual_pstate;

    XBT_INFO("Switching machine %d ('%s') OFF. Passing in virtual pstate %d to do so", machine->id,
             machine->name.c_str(), off_ps);
    MSG_host_set_pstate(machine->host, off_ps);
    //args->context->pstate_tracer.add_pstate_change(MSG_get_clock(), machine->id, off_ps);

    msg_host_t host_list[1] = {machine->host};
//    double flop_amount[1] = {1};
//    double bytes_amount[1] = {0};
    double * flop_amount = xbt_new(double, 1);
    double * bytes_amount = xbt_new(double, 1);
    flop_amount[0] = 1;
    bytes_amount[0] = 0;

    msg_task_t shutdown = MSG_parallel_task_create("switch OFF", 1, host_list, flop_amount, bytes_amount, NULL);
    XBT_INFO("Computing 1 flop to simulate time & energy cost of switch OFF");
    MSG_task_execute(shutdown);
    MSG_task_destroy(shutdown);

    XBT_INFO("1 flop has been computed. Switching machine %d ('%s') to sleeping pstate %d",
             machine->id, machine->name.c_str(), pstate);
    MSG_host_set_pstate(machine->host, pstate);
    //args->context->pstate_tracer.add_pstate_change(MSG_get_clock(), machine->id, pstate);

    machine->update_machine_state(MachineState::SLEEPING);

    SwitchMessage * msg = new SwitchMessage;
    msg->machine_id = args->machine_id;
    msg->new_pstate = args->new_pstate;
    send_message("server", IPMessageType::SWITCHED_OFF, (void *) msg);

    delete args;
    return 0;
}

void CurrentSwitches::add_switch(const MachineRange &machines, int target_pstate)
{
    Switch * s = new Switch;
    s->all_machines = machines;
    s->switching_machines = machines;
    s->target_pstate = target_pstate;

    if (_switches.count(target_pstate))
    {
        std::list<Switch *> & list = _switches[target_pstate];
        list.push_back(s);
    }
    else
    {
        _switches[target_pstate] = {s};
    }
}

bool CurrentSwitches::mark_switch_as_done(int machine_id,
                                          int target_pstate,
                                          MachineRange & all_machines,
                                          BatsimContext * context)
{
    xbt_assert(_switches.count(target_pstate) == 1);

    std::list<Switch*> & list = _switches[target_pstate];

    for (auto it = list.begin(); it != list.end(); ++it)
    {
        Switch * s = *it;

        if (s->switching_machines.contains(machine_id))
        {
            s->switching_machines.remove(machine_id);

            // If all the machines of one request have been switched
            if (s->switching_machines.size() == 0)
            {
                list.erase(it);
                // If there is no longer switches corresponding to this pstate, the pstate:list is removed from the map
                if (list.size() == 0)
                {
                    _switches.erase(target_pstate);
                }

                all_machines = s->all_machines;
                if (context->energy_used)
                {
                    context->pstate_tracer.add_pstate_change(MSG_get_clock(), s->all_machines, s->target_pstate);
                    context->energy_tracer.add_pstate_change(MSG_get_clock(), s->all_machines, s->target_pstate);
                }

                delete s;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    xbt_assert(false, "Invalid CurrentSwitches::mark_switch_as_done call: machine %d was not switching to pstate %d", machine_id, target_pstate);
    return false;
}

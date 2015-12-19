#include "pstate.hpp"

#include <simgrid/msg.h>

#include "ipp.hpp"
#include "context.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(pstate, "pstate");

int switch_on_machine_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    SwitchPStateProcessArguments * args = (SwitchPStateProcessArguments *) MSG_process_get_data(MSG_process_self());

    int machineID = args->message->machine;
    int pstate = args->message->new_pstate;

    xbt_assert(args->context->machines.exists(machineID));
    Machine * machine = args->context->machines[machineID];

    xbt_assert(machine->host->id() == MSG_process_get_host(MSG_process_self())->id());
    xbt_assert(machine->state == MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING);
    xbt_assert(machine->jobs_being_computed.empty());
    xbt_assert(machine->has_pstate(pstate));
    xbt_assert(machine->pstates[pstate] == PStateType::COMPUTATION_PSTATE);

    int on_ps = machine->sleep_pstates[pstate]->switch_on_virtual_pstate;

    XBT_INFO("Switching machine %d ('%s') ON. Passing in virtual pstate %d to do so", machine->id,
             machine->name.c_str(), on_ps);
    MSG_host_set_pstate(machine->host, on_ps);

    msg_host_t host_list[1] = {machine->host};
    double flop_amount[1] = {1};
    double bytes_amount[1] = {0};

    msg_task_t bootup = MSG_parallel_task_create("switch ON", 1, host_list, flop_amount, bytes_amount, NULL);
    XBT_INFO("Computing 1 flop to simulate time & energy cost of switch ON");
    MSG_task_execute(bootup);
    MSG_task_destroy(bootup);

    XBT_INFO("1 flop has been computed. Switching machine %d ('%s') to computing pstate %d",
             machine->id, machine->name.c_str(), pstate);
    MSG_host_set_pstate(machine->host, pstate);

    machine->state = MachineState::IDLE;

    send_message("server", IPMessageType::SWITCHED_ON, (void *) args->message);
    delete args;
    return 0;
}

int switch_off_machine_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    SwitchPStateProcessArguments * args = (SwitchPStateProcessArguments *) MSG_process_get_data(MSG_process_self());

    int machineID = args->message->machine;
    int pstate = args->message->new_pstate;

    xbt_assert(args->context->machines.exists(machineID));
    Machine * machine = args->context->machines[machineID];

    xbt_assert(machine->host->id() == MSG_process_get_host(MSG_process_self())->id());
    xbt_assert(machine->state == MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING);
    xbt_assert(machine->jobs_being_computed.empty());
    xbt_assert(machine->has_pstate(pstate));
    xbt_assert(machine->pstates[pstate] == PStateType::SLEEP_PSTATE);

    int off_ps = machine->sleep_pstates[pstate]->switch_off_virtual_pstate;

    XBT_INFO("Switching machine %d ('%s') OFF. Passing in virtual pstate %d to do so", machine->id,
             machine->name.c_str(), off_ps);
    MSG_host_set_pstate(machine->host, off_ps);

    msg_host_t host_list[1] = {machine->host};
    double flop_amount[1] = {1};
    double bytes_amount[1] = {0};

    msg_task_t shutdown = MSG_parallel_task_create("switch OFF", 1, host_list, flop_amount, bytes_amount, NULL);
    XBT_INFO("Computing 1 flop to simulate time & energy cost of switch OFF");
    MSG_task_execute(shutdown);
    MSG_task_destroy(shutdown);

    XBT_INFO("1 flop has been computed. Switching machine %d ('%s') to sleeping pstate %d",
             machine->id, machine->name.c_str(), pstate);
    MSG_host_set_pstate(machine->host, pstate);

    machine->state = MachineState::SLEEPING;

    send_message("server", IPMessageType::SWITCHED_OFF, (void *) args->message);
    delete args;
    return 0;
}

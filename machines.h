#pragma once

#include <simgrid/msg.h>

enum e_machine_state_t
{
    MACHINE_STATE_SLEEPING,
    MACHINE_STATE_IDLE,
    MACHINE_STATE_COMPUTING,
};
typedef enum e_machine_state_t MachineState;

struct s_machine_t
{
    msg_host_t host;
    MachineState state;
    xbt_dynar_t jobs_being_computed; // the IDs of the jobs being computed by the machine
};
typedef struct s_machine_t Machine;

// The array of machines
extern int nb_machines;
extern Machine* machines;

void createMachines(xbt_dynar_t hosts);
void freeMachines();

void updateMachinesOnJobRun(int jobID, int nbUsedMachines, int * usedMachines);
void updateMachinesOnJobEnd(int jobID, int nbUsedMachines, int * usedMachines);

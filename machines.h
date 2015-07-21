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
    msg_host_t host;                    //! The host the machine corresponds to
    MachineState state;                 //! The current state of the machine
    xbt_dynar_t jobs_being_computed;    //! the IDs of the jobs being computed by the machine

    int nb_computation_pstates;         //! The number of computation power states
    int * computation_pstates;          //! The computation power states (identified by their order in the SimGrid platform file)

    int nb_sleep_pstates;               //! The number of sleep power states
    int * sleep_pstates;                //! The sleep power states (identified by their order in the SimGrid platform file)

    // The transitions are stored in a naive 2D matrix, todo: use a more suited data structure to gain memory (without losing much RAM)
    int nb_pstates;                     //! The number of power states
    int * trans_pstates;                //! The transition matrix of size nb_pstates * nb_pstates stored in 1 dimension. The pstate -1 means there is no transition between two pstates. To get the transition from pstate 3 to pstate 5, access trans_pstate[3*nb_pstate+5].
};
typedef struct s_machine_t Machine;

// The array of machines
extern int nb_machines;
extern Machine* machines;

void createMachines(xbt_dynar_t hosts);
void freeMachines();

void updateMachinesOnJobRun(int jobID, int nbUsedMachines, int * usedMachines);
void updateMachinesOnJobEnd(int jobID, int nbUsedMachines, int * usedMachines);

void retrieve_pstate_information(Machine * machine);
void retrieve_comp_states_information(Machine * machine, const char * comp_states);
void retrieve_sleep_states_information(Machine * machine, const char * sleep_states);
void retrieve_trans_states_information(Machine * machine, const char * trans_states);
void check_pstates(Machine * machine);

/**
 * @brief Returns the transition pstate between two pstates on a given machine.
 * @param machine The machine
 * @param from_pstate The origin pstate
 * @param to_pstate The destination pstate
 * @return The pstate from from_pstate to to_pstate if it is defined, -1 otherwise (no transition).
 */
int get_transition_pstate(const Machine * machine, int from_pstate, int to_pstate);

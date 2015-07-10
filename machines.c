#include "machines.h"

int nb_machines = 0;
Machine * machines = NULL;

void createMachines(xbt_dynar_t hosts)
{
    xbt_assert(machines == NULL, "Bad call to createMachines(): machines already created");

    nb_machines = xbt_dynar_length(hosts);
    machines = xbt_new(Machine, nb_machines);

    msg_host_t host;
    unsigned int i;
    xbt_dynar_foreach(hosts, i, host)
    {
        Machine * machine = &machines[i];

        machine->host = host;
        machine->jobs_being_computed = xbt_dynar_new(sizeof(int), NULL);
        machine->state = MACHINE_STATE_IDLE;
    }
}

void displayDynarOfIntegers(const xbt_dynar_t d)
{
    unsigned int index;
    int value;


    unsigned int lastIndex = xbt_dynar_length(d) - 1;

    printf("[");
    xbt_dynar_foreach(d, index, value)
    {
        if (index == lastIndex)
            printf("%d", value);
        else
            printf("%d,", value);
    }

    printf("]");
}

void displayMachine(int machineID, const Machine * machine)
{
    static const char* machineStateToStr[] = {"MACHINE_STATE_SLEEPING",
                                            "MACHINE_STATE_IDLE",
                                            "MACHINE_STATE_COMPUTING"};

    printf("Machine %d, state=%s, jobs=", machineID, machineStateToStr[machine->state]);
    displayDynarOfIntegers(machine->jobs_being_computed);
    printf("\n");
    fflush(stdout);
}


void freeMachines()
{
    if (machines != NULL)
    {
        for (int i = 0; i < nb_machines; ++i)
            xbt_dynar_free(&(machines[i].jobs_being_computed));

        free(machines);
        machines = NULL;
    }
}


void updateMachinesOnJobRun(int jobID, int nbUsedMachines, int *usedMachines)
{
    for (int i = 0; i < nbUsedMachines; ++i)
    {
        int machineID = usedMachines[i];

        Machine * machine = &machines[machineID];

        machine->state = MACHINE_STATE_COMPUTING;

        //displayMachine(machineID, machine);

        // Let's update the jobs_being_computed data structure
        // This structure contains sorted job IDs
        unsigned long nbBeingComputedJobs = xbt_dynar_length(machine->jobs_being_computed);

        int inserted = 0;
        unsigned int j;
        int job;
        xbt_dynar_foreach(machine->jobs_being_computed, j, job)
        {
            if (job > jobID)
            {
                xbt_dynar_insert_at(machine->jobs_being_computed, j, &jobID);
                //displayMachine(machineID, machine);
                inserted = 1;
                break;
            }
        }

        if (!inserted)
        {
            xbt_dynar_push_as(machine->jobs_being_computed, int, jobID);
            //displayMachine(machineID, machine);
        }

        xbt_assert(xbt_dynar_length(machine->jobs_being_computed) == nbBeingComputedJobs + 1);
        xbt_assert(*((int*)xbt_dynar_get_ptr(machine->jobs_being_computed, j)) == jobID);
    }
}


void updateMachinesOnJobEnd(int jobID, int nbUsedMachines, int *usedMachines)
{
    for (int i = 0; i < nbUsedMachines; ++i)
    {
        int machineID = usedMachines[i];

        Machine * machine = &machines[machineID];

        //displayMachine(machineID, machine);

        // Let's find where jobID is in the jobs_being_computed data structure
        unsigned long nbBeingComputedJobs = xbt_dynar_length(machine->jobs_being_computed);

        int found = 0;
        unsigned int j;
        int job;
        xbt_dynar_foreach(machine->jobs_being_computed, j, job)
        {
            if (job == jobID)
            {
                xbt_dynar_remove_at(machine->jobs_being_computed, j, NULL);
                //displayMachine(machineID, machine);
                found = 1;
                break;
            }
        }

        xbt_assert(found == 1);
        xbt_assert(xbt_dynar_length(machine->jobs_being_computed) == nbBeingComputedJobs - 1);

        if (xbt_dynar_is_empty(machine->jobs_being_computed))
        {
            machine->state = MACHINE_STATE_IDLE;
            // todo: handle the Pajé trace in this file, not directly in batsim.c
        }
    }
}

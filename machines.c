#include "machines.h"

#include <string.h>

#include "batsim.h"
#include "utils.h"

int nb_machines = 0;
Machine * machines = NULL;

XBT_LOG_NEW_DEFAULT_CATEGORY(machines, "machines");

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

        machine->id = i;
        machine->host = host;
        machine->jobs_being_computed = xbt_dynar_new(sizeof(int), NULL);
        machine->state = MACHINE_STATE_IDLE;
        MSG_host_set_data(machine->host, machine);

        retrieve_pstate_information(machine);
    }
}

int int_comparator(const void * a, const void * b)
{
    const int * ia = (const int *) a;
    const int * ib = (const int *) b;

    return *ia - *ib;
}

void retrieve_pstate_information(Machine * machine)
{
    // this guard avoids the program to crash because MSG_host_get_nb_pstates is unimplemented for model ptask_L07 at the moment (2015-09-24)
    // todo: implement the function in SimGrid
    if (1)
    {
        machine->computation_pstates = 0;
        machine->sleep_pstates = 0;
        machine->trans_pstates = 0;
        return;
    }

    machine->nb_pstates = MSG_host_get_nb_pstates(machine->host);
    xbt_assert(machine->nb_pstates >= 1);

    const char * comp_states = MSG_host_get_property_value(machine->host, "comp_states");
    const char * sleep_states = MSG_host_get_property_value(machine->host, "sleep_states");
    const char * trans_states = MSG_host_get_property_value(machine->host, "trans_states");

    retrieve_comp_states_information(machine, comp_states);
    retrieve_sleep_states_information(machine, sleep_states);
    retrieve_trans_states_information(machine, trans_states);

    // Let's handle the (default) case: when nothing is given about computation and sleep pstates.
    // Let's set all pstates whose computational power is not null as comp_states
    // and set the other ones as sleep_states
    if (!comp_states && !sleep_states)
    {
        // Let's count the number of states
        machine->nb_computation_pstates = 0;
        machine->nb_sleep_pstates = 0;

        for (int i = 0; i < machine->nb_pstates; ++i)
        {
            double computational_power = MSG_host_get_power_peak_at(machine->host, i);
            if (computational_power == 0)
                machine->nb_sleep_pstates++;
            else
                machine->nb_computation_pstates++;
        }

        // Let's create the arrays if needed
        if (machine->nb_computation_pstates > 0)
            machine->computation_pstates = xbt_new(int, machine->nb_computation_pstates);

        if (machine->nb_sleep_pstates)
            machine->sleep_pstates = xbt_new(int, machine->nb_sleep_pstates);

        // Let's traverse the states again to put data in the arrays
        int it_comp = 0;
        int it_sleep = 0;

        for (int i = 0; i < machine->nb_pstates; ++i)
        {
            double computational_power = MSG_host_get_power_peak_at(machine->host, i);
            if (computational_power == 0)
                machine->sleep_pstates[it_sleep++] = i;
            else
                machine->computation_pstates[it_comp++] = i;
        }
    }

    check_pstates(machine);
}

void retrieve_comp_states_information(Machine *machine, const char * comp_states)
{
    machine->nb_computation_pstates = 0;
    machine->computation_pstates = NULL;

    if (comp_states)
    {
        // Let's check if the string is empty (discarding space characters)
        char * trimmedWholeString = get_trimmed_string(comp_states);
        int trimmedWholeStringLength = strlen(trimmedWholeString);
        free(trimmedWholeString);

        if (trimmedWholeStringLength > 0)
        {
            // Let's split the value of the property by ',' to read all the values
            xbt_dynar_t parts = xbt_str_split(comp_states, ",");

            machine->nb_computation_pstates = xbt_dynar_length(parts);
            xbt_assert(machine->nb_computation_pstates >= 1);
            machine->computation_pstates = xbt_new(int, machine->nb_computation_pstates);

            char * part;
            unsigned int i;
            xbt_dynar_foreach(parts, i, part)
            {
                long int pstate;
                int conversionWorked = get_long_from_string(part, &pstate);
                xbt_assert(conversionWorked,
                           "Invalid property comp_states of host '%s': the part '%s' cannot be converted to an integer",
                           MSG_host_get_name(machine->host), part);

                xbt_assert(pstate >= 0 && pstate < machine->nb_pstates, "Invalid pstate (%ld) in the property comp_states of host '%s': must be in [0,%d[",
                           pstate, MSG_host_get_name(machine->host), machine->nb_pstates);

                machine->computation_pstates[i] = pstate;
            }

            xbt_dynar_free(&parts);

            // Let's sort the values then ensure their unicity
            qsort(machine->computation_pstates, machine->nb_computation_pstates, sizeof(int), int_comparator);

            int nb_pstates_m1 = machine->nb_computation_pstates - 1;
            for (int j = 0; j < nb_pstates_m1; ++j)
            {
                xbt_assert(machine->computation_pstates[j] < machine->computation_pstates[j+1],
                        "Invalid property comp_states of host '%s': duplication of pstate %d",
                        MSG_host_get_name(machine->host), machine->computation_pstates[j]);
            }
        }
    }
}

void retrieve_sleep_states_information(Machine *machine, const char *sleep_states)
{
    machine->nb_sleep_pstates = 0;
    machine->sleep_pstates = NULL;

    if (sleep_states)
    {
        // Let's check if the string is empty (discarding space characters)
        char * trimmedWholeString = get_trimmed_string(sleep_states);
        int trimmedWholeStringLength = strlen(trimmedWholeString);
        free(trimmedWholeString);

        if (trimmedWholeStringLength > 0)
        {
            // Let's split the value of the property by ',' to read all the values
            xbt_dynar_t parts = xbt_str_split(sleep_states, ",");

            machine->nb_sleep_pstates = xbt_dynar_length(parts);
            xbt_assert(machine->nb_sleep_pstates >= 1);
            machine->sleep_pstates = xbt_new(int, machine->nb_sleep_pstates);

            char * part;
            unsigned int i;
            xbt_dynar_foreach(parts, i, part)
            {
                long int pstate;
                int conversionWorked = get_long_from_string(part, &pstate);

                xbt_assert(conversionWorked,
                           "Invalid property sleep_states of host '%s': the part '%s' cannot be converted to an integer",
                           MSG_host_get_name(machine->host), part);

                xbt_assert(pstate >= 0 && pstate < machine->nb_pstates, "Invalid pstate (%ld) in the property sleep_states of host '%s': must be in [0,%d[",
                           pstate, MSG_host_get_name(machine->host), machine->nb_pstates);

                machine->sleep_pstates[i] = pstate;
            }

            xbt_dynar_free(&parts);

            // Let's sort the values then ensure their unicity
            qsort(machine->sleep_pstates, machine->nb_sleep_pstates, sizeof(int), int_comparator);

            int nb_pstates_m1 = machine->nb_sleep_pstates - 1;
            for (int j = 0; j < nb_pstates_m1; ++j)
            {
                xbt_assert(machine->sleep_pstates[j] < machine->sleep_pstates[j+1],
                        "Invalid property sleep_states of host '%s': duplication of pstate %d",
                        MSG_host_get_name(machine->host), machine->sleep_pstates[j]);
            }
        }
    }
}


void retrieve_trans_states_information(Machine *machine, const char *trans_states)
{
    // Let's create the matrix and fill it with -1
    int squared_nb_pstates = machine->nb_pstates * machine->nb_pstates;
    machine->trans_pstates = xbt_new(int, squared_nb_pstates);

    for (int i = 0; i < squared_nb_pstates; ++i)
        machine->trans_pstates[i] = -1;

    if (trans_states)
    {
        // Let's check if the string is empty (discarding space characters)
        char * trimmedWholeString = get_trimmed_string(trans_states);
        int trimmedWholeStringLength = strlen(trimmedWholeString);
        free(trimmedWholeString);

        if (trimmedWholeStringLength > 0)
        {
            // Let's split the value of the property by ',' to read all the values
            xbt_dynar_t parts = xbt_str_split(trans_states, ",");

            char * part;
            unsigned int i;
            xbt_dynar_foreach(parts, i, part)
            {
                // Let's split the part in subparts via ':'
                xbt_dynar_t subparts = xbt_str_split(part, ":");

                xbt_assert(xbt_dynar_length(subparts) == 3,
                           "Invalid property trans_pstates of host '%s': the %u-th part '%s' is invalid, it must follow the syntax "
                           "'SOURCE:DESTINATION:TRANSITION' where these values are pstates (non-negative integers)",
                           MSG_host_get_name(machine->host), i, part);

                int values[3];

                char * subpart;
                unsigned int j;
                xbt_dynar_foreach(subparts, j, subpart)
                {
                    long int pstate;
                    int conversionWorked = get_long_from_string(subpart, &pstate);

                    xbt_assert(conversionWorked,
                               "Invalid property trans_pstates of host '%s': invalid %u-th part '%s': "
                               "invalid %u-th subpart '%s': cannot be converted to an integer",
                               MSG_host_get_name(machine->host), i, part, j, subpart);

                    xbt_assert(pstate >= 0 && pstate < machine->nb_pstates,
                               "Invalid property trans_pstates of host '%s': invalid %u-th part '%s': "
                               "invalid %u-th subpart '%s': converted value (%ld) must be in range [0:%d[",
                               MSG_host_get_name(machine->host), i, part, j, subpart, pstate,
                               machine->nb_pstates);

                    values[j] = pstate;
                }

                xbt_dynar_free(&subparts);

                int from_pstate = values[0];
                int to_pstate = values[1];
                int via_pstate = values[2];

                xbt_assert((from_pstate != to_pstate) && (from_pstate != via_pstate) && (to_pstate != via_pstate),
                           "Invalid property trans_pstates of host '%s': invalid %u-th part '%s': duplication of pstates",
                           MSG_host_get_name(machine->host), i, part);

                int arrayIndex = from_pstate * machine->nb_pstates + to_pstate;
                xbt_assert(machine->trans_pstates[arrayIndex] == -1,
                           "Invalid property trans_pstates of host '%s': invalid %u-th part '%s': "
                           "multiple definition of transition from pstate %d to pstate %d",
                           MSG_host_get_name(machine->host), i, part, from_pstate, to_pstate);

                machine->trans_pstates[arrayIndex] = via_pstate;
            }

            xbt_dynar_free(&parts);
        }
    }

}

void check_pstates(Machine *machine)
{
    // Let's check that there is no duplication (a pstate can either be a computation one,
    // a sleeping one or a transition one but not a combinaison of those).

    // Let's merge the computation and sleep pstates in the same array
    int nb_comp_sleep_pstates = machine->nb_computation_pstates + machine->nb_sleep_pstates;
    int * comp_sleep_array = xbt_new(int, nb_comp_sleep_pstates);
    memcpy(comp_sleep_array, machine->computation_pstates, sizeof(int) * machine->nb_computation_pstates);
    memcpy(comp_sleep_array + machine->nb_computation_pstates, machine->sleep_pstates, sizeof(int) * machine->nb_sleep_pstates);

    // Let's sort this array then ensure unicity of values
    qsort(comp_sleep_array, machine->nb_computation_pstates + machine->nb_sleep_pstates, sizeof(int), int_comparator);

    int nb_comp_sleep_pstates_m1 = nb_comp_sleep_pstates - 1;
    for (int i = 0; i < nb_comp_sleep_pstates_m1; ++i)
    {
        xbt_assert(comp_sleep_array[i] < comp_sleep_array[i+1],
                "Invalid pstates of host '%s': pstate %d cannot be a computation state AND a sleep state",
                MSG_host_get_name(machine->host), comp_sleep_array[i]);
    }

    // Okay, there is no state being a computation one and a sleep one.
    // Let's ensure that only the transition pstates (those not in the merged array) are used as transitions

    int arrayIndex = 0;
    for (int i = 0; i < machine->nb_pstates; ++i)
    {
        for (int j = 0; j < machine->nb_pstates; ++j)
        {
            int via_pstate = machine->trans_pstates[arrayIndex++];
            if (via_pstate != -1)
            {
                int found = 0;
                for (int k = 0; k < nb_comp_sleep_pstates && comp_sleep_array[k] <= via_pstate; ++k)
                {
                    if (comp_sleep_array[k] == via_pstate)
                        found = 1;
                }

                xbt_assert(!found, "Invalid pstates of host '%s': pstate %d is used as a transition pstate (from pstate %d "
                           "to pstate %d) but is defined either as a computation pstate or as a sleep one, which is invalid",
                           MSG_host_get_name(machine->host), via_pstate, i, j);
            }
        }
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
        {
            xbt_dynar_free(&(machines[i].jobs_being_computed));
            free(machines[i].computation_pstates);
            free(machines[i].sleep_pstates);
            free(machines[i].trans_pstates);
        }

        free(machines);
        machines = NULL;
    }
}


void updateMachinesOnJobRun(int jobID, int nbUsedMachines, int *usedMachines, PajeTracer *tracer)
{
    pajeTracer_addJobLaunching(tracer, MSG_get_clock(), jobID, nbUsedMachines, usedMachines);
    pajeTracer_addJobRunning(tracer, MSG_get_clock(), jobID, nbUsedMachines, usedMachines);
    // todo: better handle the trace file

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


void updateMachinesOnJobEnd(int jobID, int nbUsedMachines, int *usedMachines, PajeTracer *tracer, bool killed)
{
    pajeTracer_addJobEnding(tracer, MSG_get_clock(), jobID, nbUsedMachines, usedMachines);

    if (killed)
        pajeTracer_addJobKill(tracer, MSG_get_clock(), jobID, nbUsedMachines, usedMachines);

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
            // todo: better handle the Pajé trace here rather than the global pajeTracer_addJobEnding
        }
    }
}


int get_transition_pstate(const Machine *machine, int from_pstate, int to_pstate)
{
    int arrayIndex = from_pstate * machine->nb_pstates + to_pstate;
    xbt_assert(arrayIndex >= 0 && arrayIndex < machine->nb_pstates * machine->nb_pstates);

    return machine->trans_pstates[arrayIndex];
}

void update_machine_pstate(int machineID, int pstate)
{
    xbt_assert(machineID >= 0 && machineID < nb_machines, "Invalid parameter machineID (%d) : must be in [0;%d[", machineID, nb_machines);

    Machine * machine = machines + machineID;
    xbt_assert(machine->state == MACHINE_STATE_TRANSITING);
    int current_pstate = MSG_host_get_pstate(machine->host);

    xbt_assert(pstate >= 0 && pstate < machine->nb_pstates, "Invalid parameter pstate (%d) : must be in [0:%d[ for machine %d", pstate, machine->nb_pstates, machineID);
    xbt_assert(pstate != current_pstate, "Invalid parameter pstate (%d) : the machine %d is already in this pstate", pstate, machineID);

    int transition = machine->trans_pstates[current_pstate*machine->nb_pstates+pstate];

    // If a transition must be done
    if (transition != -1)
    {
        xbt_assert(xbt_dynar_length(machine->jobs_being_computed) == 0,
            "Jobs cannot be suspended and resumed at the moment. "
            "Please only change the pstate of idle machines (machine %d is currently running %lu jobs",
            machineID, xbt_dynar_length(machine->jobs_being_computed));

        MSG_host_set_pstate(machine->host, transition);

        msg_host_t host_list [1] = {machine->host};
        double flops_amount [1] = {1};
        double bytes_amount [1] = {0};
        msg_task_t transition_task = MSG_parallel_task_create("shutdown", 1, host_list, flops_amount, bytes_amount, NULL);

        XBT_INFO("Machine %d starts its transition from pstate %d to pstate %d", machineID, current_pstate, pstate);
        MSG_task_execute(transition_task);
        MSG_task_destroy(transition_task);
    }

    if (is_sleep_pstate(machineID, pstate))
        machine->state = MACHINE_STATE_SLEEPING;
    else
        machine->state = MACHINE_STATE_IDLE;

    MSG_host_set_pstate(machine->host, pstate);
    XBT_INFO ("Machine %d finished its transition from pstate %d to pstate %d", machineID, current_pstate, pstate);

    send_message("server", MACHINE_PSTATE_CHANGED, machineID, NULL);
}

bool is_sleep_pstate(int machineID, int pstate)
{
    xbt_assert(machineID >= 0 && machineID < nb_machines, "Invalid parameter machineID (%d) : must be in [0;%d[", machineID, nb_machines);

    Machine * machine = machines + machineID;
    xbt_assert(pstate >= 0 && pstate < machine->nb_pstates, "Invalid parameter pstate (%d) : must be in [0:%d[ for machine %d", pstate, machine->nb_pstates, machineID);

    for (int i = 0; i < machine->nb_sleep_pstates; ++i)
        if (machine->sleep_pstates[i] == pstate)
            return true;

    return false;
}

bool machine_exists(int machineID)
{
    return machineID >= 0 && machineID < nb_machines;
}

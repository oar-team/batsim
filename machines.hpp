#pragma once

#include <vector>
#include <set>
#include <map>
#include <string>

#include <simgrid/msg.h>

#include "pstate.hpp"

class PajeTracer;
struct BatsimContext;

enum class MachineState
{
    SLEEPING,
    IDLE,
    COMPUTING,
    TRANSITING_FROM_SLEEPING_TO_COMPUTING,
    TRANSITING_FROM_COMPUTING_TO_SLEEPING,
};

struct Machine
{
    ~Machine();

    int id;
    std::string name;
    msg_host_t host;
    MachineState state;
    std::set<int> jobs_being_computed;

    std::map<int, PStateType> pstates;
    std::map<int, SleepPState *> sleep_pstates;

    bool has_pstate(int pstate) const;
    void display_machine(bool is_energy_used) const;
    std::string jobs_being_computed_as_string() const;
};

class Machines
{
public:
    Machines();
    ~Machines();
    void createMachines(xbt_dynar_t hosts, BatsimContext * context, const std::string & masterHostName);
    void updateMachinesOnJobRun(int jobID, const std::vector<int> & usedMachines);
    void updateMachinesOnJobEnd(int jobID, const std::vector<int> & usedMachines);

    void setTracer(PajeTracer * tracer);

    const Machine * operator[](int machineID) const;
    Machine * operator[](int machineID);

    bool exists(int machineID) const;
    void displayDebug() const;

    const std::vector<Machine *> & machines() const;
    const Machine * masterMachine() const;

private:
    std::vector<Machine *> _machines;
    Machine * _masterMachine = nullptr;
    PajeTracer * _tracer = nullptr;
};

std::string machineStateToString(MachineState state);

// The array of machines
extern int nb_machines;
extern Machine* machines;

void createMachines(xbt_dynar_t hosts);
void freeMachines();

void updateMachinesOnJobRun(int jobID, int nbUsedMachines, int * usedMachines);
void updateMachinesOnJobEnd(int jobID, int nbUsedMachines, int * usedMachines);

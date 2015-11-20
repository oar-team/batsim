#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <string>

#include <simgrid/msg.h>

enum class MachineState
{
    SLEEPING,
    IDLE,
    COMPUTING,
};

struct Machine
{
    int id;
    std::string name;
    msg_host_t host;
    MachineState state;
    std::set<int> jobs_being_computed;
};

std::ostream & operator<<(std::ostream & out, const Machine & machine);

class Machines
{
public:
    Machines();
    ~Machines();
    void createMachines(xbt_dynar_t hosts);
    void updateMachinesOnJobRun(int jobID, const std::vector<int> & usedMachines);
    void updateMachinesOnJobEnd(int jobID, const std::vector<int> & usedMachines);

    const Machine * operator[](int machineID) const;
    Machine * operator[](int machineID);

    bool exists(int machineID) const;

private:
    std::vector<Machine *> _machines;
};

std::string machineStateToString(MachineState state);

// The array of machines
extern int nb_machines;
extern Machine* machines;

void createMachines(xbt_dynar_t hosts);
void freeMachines();

void updateMachinesOnJobRun(int jobID, int nbUsedMachines, int * usedMachines);
void updateMachinesOnJobEnd(int jobID, int nbUsedMachines, int * usedMachines);

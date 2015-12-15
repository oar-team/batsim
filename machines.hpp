#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <string>

#include <simgrid/msg.h>

class PajeTracer;

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
    void createMachines(xbt_dynar_t hosts, const std::string & masterHostName);
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

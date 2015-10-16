#include "machines.hpp"

#include <algorithm>
#include <iterator>

using namespace std;

Machines::Machines()
{

}

Machines::~Machines()
{
    _machines.clear();
}

void Machines::createMachines(xbt_dynar_t hosts)
{
    xbt_assert(_machines.size() == 0, "Bad call to Machines::createMachines(): machines already created");

    int nb_machines = xbt_dynar_length(hosts);
    _machines.resize(nb_machines);

    msg_host_t host;
    unsigned int i;
    xbt_dynar_foreach(hosts, i, host)
    {
        Machine & machine = _machines[i];

        machine.id = i;
        machine.host = host;
        machine.jobs_being_computed = {};
        machine.state = machine_state::IDLE;
    }
}

const Machine & Machines::operator[](int machineID) const
{
    return _machines[machineID];
}

Machine & Machines::operator[](int machineID)
{
    return _machines[machineID];
}

void Machines::updateMachinesOnJobRun(int jobID, const std::vector<int> & usedMachines)
{
    for (int machineID : usedMachines)
    {
        Machine & machine = _machines[machineID];
        machine.state = machine_state::COMPUTING;

        // cout << machine;
        machine.jobs_being_computed.insert(jobID);
        // cout << machine;
    }
}

void Machines::updateMachinesOnJobEnd(int jobID, const std::vector<int> & usedMachines)
{
    for (int machineID : usedMachines)
    {
        Machine & machine = _machines[machineID];
        // cout << machine;

        // Let's find where jobID is in the jobs_being_computed data structure
        int ret = machine.jobs_being_computed.erase(jobID);
        xbt_assert(ret == 1);

        if (machine.jobs_being_computed.empty())
        {
            machine.state = machine_state::IDLE;
            // todo: handle the Pajé trace in this file, not directly in batsim.c
        }

        // cout << machine;
    }
}

ostream & operator<<(ostream & out, const Machine & machine)
{
    static const char* machineStateToStr[] = {"SLEEPING",
                                              "IDLE",
                                              "COMPUTING"};

    out << "Machine " << machine.id << ", ";
    out << "state = " << machineStateToStr[machine.state] << ", ";
    out << "jobs = [";
    std::copy(machine.jobs_being_computed.begin(), machine.jobs_being_computed.end(),
              std::ostream_iterator<char>(out, " "));
    out << "]" << endl;

    return out;
}

int main()
{
    return 0;
}
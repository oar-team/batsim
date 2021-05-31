
#include "probe.hpp"

#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include <simgrid/host.h>
#include <simgrid/plugins/energy.h>

#include "batsim.hpp"
#include "context.hpp"
#include "export.hpp"
#include "jobs.hpp"
#include "permissions.hpp"


using namespace std;
using namespace roles;



Probe new_probe(std::string name,const IntervalSet & machines){
    Probe nwprobe;
    nwprobe.name = name;
    nwprobe.id_machines = machines;
    return nwprobe;
}

long double Probe::get_consumed_energy(BatsimContext * context){
    if (!context->energy_used)
    {
        return 0;
    }

    long double consumed_energy = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        consumed_energy += static_cast<long double>(sg_host_get_consumed_energy(machine->host));
    }

    return consumed_energy;
}


double Probe::power_consumption(BatsimContext * context){
    if (!context->energy_used)
    {
        return 0;
    }
    double current_consumption = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        current_consumption += static_cast<double>(sg_host_get_current_consumption(machine->host));
    }
    return current_consumption;
}



#include "probe.hpp"

#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include <simgrid/host.h>
#include <simgrid/plugins/energy.h>
#include <simgrid/plugins/load.h>

#include "batsim.hpp"
#include "context.hpp"
#include "export.hpp"
#include "jobs.hpp"
#include "permissions.hpp"


using namespace std;
using namespace roles;



Probe new_probe(std::string name,Metric metrics ,const IntervalSet & machines, BatsimContext * context){
    Probe nwprobe;
    nwprobe.name = name;
    xbt_assert(metrics != Metric::UNKNOWN, "A metric is not recognized by batsim");
    nwprobe.metrics = metrics;
    nwprobe.id_machines = machines;
    nwprobe.context = context;
    return nwprobe;
}

double Probe::consumed_energy(){
    if (!context->energy_used)
    {
        return 0;
    }

     double consumed_energy = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        consumed_energy += sg_host_get_consumed_energy(machine->host);
    }

    return static_cast<double>(consumed_energy);
}


double Probe::power_consumption(){
    if (!context->energy_used)
    {
        return 0;
    }

    double current_consumption = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        current_consumption += sg_host_get_current_consumption(machine->host);
    }
    return current_consumption;
}

double Probe::current_load(){
    if (!context->energy_used)
    {
        return 0;
    }

    double current_load = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        current_load += sg_host_get_current_load(machine->host);
    }
    return current_load;
}

double Probe::average_load(){
    if (!context->energy_used)
    {
        return 0;
    }

    double average_load = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        average_load += sg_host_get_avg_load(machine->host);
    }
    return average_load;
}

double Probe::return_value(){ 
    sg_host_energy_update_all();
    double value = 0;
    switch(metrics)
    {
        case Metric::CONSUMED_ENERGY :
            value = consumed_energy();
            break;
        case Metric::POWER_CONSUMPTION :
            value = power_consumption();
            break;
        case Metric::CURRENT_LOAD :
            value = current_load();
            break;
        case Metric::AVERAGE_LOAD :
            value = average_load();
            break;
        default:
            break;
    }
    return value;
}


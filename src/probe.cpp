
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



Probe new_probe(std::string name,std::string metrics ,const IntervalSet & machines, BatsimContext * context){
    Probe nwprobe;
    nwprobe.name = name;
    nwprobe.metrics = metrics;
    nwprobe.id_machines = machines;
    nwprobe.context = context;
    return nwprobe;
}

double Probe::get_consumed_energy(){
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

double Probe::return_value(){ 
    double value = 0;
    if(metrics.compare("power consumption")){
        value = power_consumption();
    }
    else if (metrics.compare("energy consumed")){
        value = get_consumed_energy();
    }
    return value;
}


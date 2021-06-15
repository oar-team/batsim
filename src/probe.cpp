
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



Probe new_probe(std::string name,Metrics metrics, TypeOfAggregation type, bool agg ,const IntervalSet & machines, BatsimContext * context){
    Probe nwprobe;
    nwprobe.name = name;
    xbt_assert(metrics != Metrics::UNKNOWN, "A metric is not recognized by batsim");
    nwprobe.metrics = metrics;
    nwprobe.aggregate = agg;
    if(agg){
        xbt_assert(type != TypeOfAggregation::UNKNOWN && type!= TypeOfAggregation::NONE, "The type of aggregation is not recognized by batsim");
        nwprobe.type_of_aggregation = type;
    }
    nwprobe.id_machines = machines;
    nwprobe.context = context;
    return nwprobe;
}

double Probe::consumed_energy(int machine_id){
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_consumed_energy(machine->host);
    return res;
}

double Probe::average_consumed_energy(){
    int size = id_machines.size();
    double res = added_consumed_energy() / size;
    return res;
}

    
double Probe::average_power_consumption(){
    int size = id_machines.size();
    double res = added_power_consumption() / size;
    return res;
}


   
double Probe::average_current_load(){
    int size = id_machines.size();
    double res = added_current_load() / size;
    return res;
}
   
double Probe::average_agg_load(){
    int size = id_machines.size();
    double res = added_current_load() / size;
    return res;
}

    
double Probe::minimum_consumed_energy(){
    double res = consumed_energy(*(id_machines.elements_begin()));
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = consumed_energy(machine_id);
        if (intermediate < res){
            res = intermediate;
        }
    }
    return res;
}

    
double Probe::minimum_power_consumption(){
    double res = power_consumption(*(id_machines.elements_begin()));
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = power_consumption(machine_id);
        if (intermediate < res){
            res = intermediate;
        }
    }
    return res;
}

    
double Probe::minimum_current_load(){
    double res = current_load(*(id_machines.elements_begin()));
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = current_load(machine_id);
        if (intermediate < res){
            res = intermediate;
        }
    }
    return res;
}

   
double Probe::minimum_agg_load(){
    double res = average_load(*(id_machines.elements_begin()));
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = average_load(machine_id);
        if (intermediate < res){
            res = intermediate;
        }
    }
    return res;
}


double Probe::maximum_consumed_energy(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = consumed_energy(machine_id);
        if (intermediate > res){
            res = intermediate;
        }
    }
    return res;
}

  
double Probe::maximum_power_consumption(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = power_consumption(machine_id);
        if (intermediate > res){
            res = intermediate;
        }
    }
    return res;
}

  
double Probe::maximum_current_load(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = current_load(machine_id);
        if (intermediate > res){
            res = intermediate;
        }
    }
    return res;
}

double Probe::maximum_agg_load(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        double intermediate = average_load(machine_id);
        if (intermediate > res){
            res = intermediate;
        }
    }
    return res;
}

  
// double Probe::median_consumed_energy(){
//     int size = id_machines.size();
//     double *
// }

 
// double Probe::median_power_consumption();

  
// double Probe::median_current_load();

  
// double Probe::median_agg_load();



double Probe::added_consumed_energy(){
    if (!context->energy_used)
    {
        return 0;
    }

    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        res += consumed_energy(machine_id);
    }

    return res;
}

double Probe::power_consumption(int machine_id){
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_current_consumption(machine->host);
    return res;
}


double Probe::added_power_consumption(){
    if (!context->load_used)
    {
        return 0;
    }

    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        res += power_consumption(machine_id);
    }
    return res;
}

double Probe::current_load(int machine_id){
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_current_load(machine->host);
    return res;
}

double Probe::added_current_load(){
    if (!context->energy_used)
    {
        xbt_assert(false,"le probleme est la");
        return 0;
    }

    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        res += current_load(machine_id);
    }
    return res;
}

double Probe::average_load(int machine_id){
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_avg_load(machine->host);
    return res;
}

double Probe::added_average_load(){
    if (!context->load_used)
    {
        xbt_assert(false,"le probleme est la");
        return 0;
    }

    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        res += average_load(machine_id);
    }
    return res;
}




double Probe::aggregate_addition(){ 
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = added_consumed_energy();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = added_power_consumption();
            break;
        case Metrics::CURRENT_LOAD :
            res = added_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = added_average_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_minimum(){
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = minimum_consumed_energy();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = minimum_power_consumption();
            break;
        case Metrics::CURRENT_LOAD :
            res = minimum_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = minimum_agg_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_maximum(){
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = maximum_consumed_energy();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = maximum_power_consumption();
            break;
        case Metrics::CURRENT_LOAD :
            res = maximum_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = maximum_agg_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_average(){
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = average_consumed_energy();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = average_power_consumption();
            break;
        case Metrics::CURRENT_LOAD :
            res = average_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = average_agg_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_value(){
    double res =0;
    switch(type_of_aggregation){
        case TypeOfAggregation::ADDITION :
            res = aggregate_addition();
            break;
        case TypeOfAggregation::MINIMUM :
            res = aggregate_minimum();
            break;
        case TypeOfAggregation::MAXIMUM :
            res = aggregate_maximum();
            break;
        case TypeOfAggregation::AVERAGE :
            res = aggregate_average();
            break;
        default :
            break;
    }
    return res;
}

vector<DetailedData> Probe::detailed_consumed_energy(){
    std::vector<DetailedData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedData to_add;
        to_add.id= id;
        to_add.value = consumed_energy(id);
        res.push_back(to_add);
    }
    return res;
}

vector<DetailedData> Probe::detailed_power_consumption(){
    std::vector<DetailedData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedData to_add;
        to_add.id= id;
        to_add.value = power_consumption(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<DetailedData> Probe::detailed_current_load(){
    std::vector<DetailedData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedData to_add;
        to_add.id= id;
        to_add.value = current_load(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<DetailedData> Probe::detailed_average_load(){
    std::vector<DetailedData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedData to_add;
        to_add.id= id;
        to_add.value = average_load(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<DetailedData> Probe::detailed_value(){
    std::vector<DetailedData> res;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = detailed_consumed_energy();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = detailed_power_consumption();
            break;
        case Metrics::CURRENT_LOAD :
            res = detailed_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = detailed_average_load();
            break;
        default:
            break;
    }
    return res;
}

std::string  aggregation_to_string(TypeOfAggregation type){
    std::string res ="";
    switch(type){
        case TypeOfAggregation::ADDITION :
            res = "addition";
            break;
        case TypeOfAggregation::MINIMUM :
            res = "minimum";
            break;
        case TypeOfAggregation::MAXIMUM :
            res = "maximum";
            break;
        case TypeOfAggregation::AVERAGE :
            res = "average";
            break;
        default :
            break;
    }
    return res;
}




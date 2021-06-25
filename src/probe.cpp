
#include "probe.hpp"

#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include <simgrid/host.h>
#include <simgrid/plugins/energy.h>
#include <simgrid/plugins/load.h>
#include <simgrid/s4u/Link.hpp>


#include "batsim.hpp"
#include "context.hpp"
#include "export.hpp"
#include "jobs.hpp"
#include "permissions.hpp"


using namespace std;
using namespace roles;

void verif_name(BatsimContext* context, std::string name){
    vector<Probe> probes = context->probes;
    for (unsigned i =0 ; i < probes.size(); i++){
        if((probes[i].name).compare(name)==0){
            xbt_die("This name is already taken by another probe");
        }
    }
}

Probe new_host_probe(std::string name, TypeOfTrigger trigger, Metrics metrics, TypeOfAggregation agg,const IntervalSet & machines, BatsimContext * context){
    verif_name(context,name);
    Probe nwprobe;
    nwprobe.object = TypeOfObject::HOST;
    nwprobe.name = name;
    nwprobe.trigger = trigger;
    xbt_assert(metrics != Metrics::UNKNOWN, "A metric is not recognized by batsim");
    nwprobe.metrics = metrics;
    nwprobe.aggregation = agg;
    nwprobe.id_machines = machines;
    nwprobe.context = context;
    context->probes.push_back(nwprobe);
    return nwprobe;
}

Probe new_link_probe(std::string name, TypeOfTrigger trigger, Metrics met, TypeOfAggregation agg, vector<std::string> links_name, BatsimContext * context){
    verif_name(context,name);
    Probe nwprobe;
    nwprobe.object = TypeOfObject::LINK;
    nwprobe.name = name;
    nwprobe.trigger = trigger;
    xbt_assert(met != Metrics::UNKNOWN && met != Metrics::POWER_CONSUMPTION, "A metric is not recognized by batsim");
    nwprobe.metrics = met;
    nwprobe.aggregation = agg;
    nwprobe.context = context;
    vector<simgrid::s4u::Link*> links_to_add;
    for(unsigned i = 0 ; i < links_name.size();i++){
        simgrid::s4u::Link* link = simgrid::s4u::Link::by_name(links_name[i]);
        links_to_add.push_back(link);
    }
    nwprobe.links = links_to_add;
    context->probes.push_back(nwprobe);
    return nwprobe;
}


void Probe::activation(){
    if(object == TypeOfObject::LINK){
                track_links();
            }
    switch(trigger){
        case TypeOfTrigger::ONE_SHOT :
            one_shot_reaction();
            break;
        default :
            xbt_die("Unsupported or not yet implemented type of trigger");
    }
}

void Probe::destruction(){
    vector<Probe> probes = context->probes;
    Probe probe;
    switch(object){
        case TypeOfObject::LINK :
            untrack_links();
            break;
        default :
            for (unsigned i =0 ; i< probes.size(); i++){
                probe = probes[i];
                if((probe.name).compare(name)==0){
                    probes.erase(probes.begin()+i);
                    break;
                }
            }
            break;
    }
}

void Probe::track_links(){
    if(object != TypeOfObject::LINK){
        xbt_die("This probe does not work on links");
    }
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        sg_link_load_track(link);
    }
}

void Probe::untrack_links(){
    if(object != TypeOfObject::LINK){
        xbt_die("This probe does not work on links");
    }
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        sg_link_load_untrack(link);
    }
}

void Probe::one_shot_reaction(){
    vector<DetailedHostData> host_value;
    vector<DetailedLinkData> link_value;
    double value;
    switch(aggregation){
        case TypeOfAggregation::NONE :
            switch(object){
                case TypeOfObject::HOST :
                    host_value = detailed_value();
                    context->proto_writer->append_detailed_probe_data(name,simgrid::s4u::Engine::get_clock(),host_value,metrics);
                    destruction();
                    break;
                case TypeOfObject::LINK :
                    link_value = link_detailed_value();
                    context->proto_writer->append_detailed_link_probe_data(name,simgrid::s4u::Engine::get_clock(),link_value,metrics);
                    destruction();
                    break;
                default :
                    xbt_die("Unsupported type of object");
                    break;
            }
            break;
        case TypeOfAggregation::UNKNOWN :
            xbt_die("Unsupported type of aggregation");
            break;
        default :
            value = aggregate_value();
            context->proto_writer->append_aggregate_probe_data(name,simgrid::s4u::Engine::get_clock(),value,aggregation,metrics);
            destruction();
            break;
    }
}

double Probe::consumed_energy(int machine_id){
    xbt_assert(context->energy_used,"The energy plugin has not been initialized");
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

bool operator<(DetailedHostData const &a, DetailedHostData const &b){
    if(a.value < b.value){
        return true;
    }
    else {
        return false;
    }
}

double Probe::median_consumed_energy(){
    vector<DetailedHostData> vec = detailed_consumed_energy();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_power_consumption(){
    vector<DetailedHostData> vec = detailed_power_consumption();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_current_load(){
    vector<DetailedHostData> vec = detailed_current_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_agg_load(){
    vector<DetailedHostData> vec = detailed_average_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
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

  

double Probe::added_consumed_energy(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        res += consumed_energy(machine_id);
    }

    return res;
}

double Probe::power_consumption(int machine_id){
    xbt_assert(context->energy_used,"The energy plugin has not been initialized");
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_current_consumption(machine->host);
    return res;
}


double Probe::added_power_consumption(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        res += power_consumption(machine_id);
    }
    return res;
}

double Probe::current_load(int machine_id){
    xbt_assert(context->load_used, "The load plugin has not been initialized");
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_current_load(machine->host);
    return res;
}

double Probe::added_current_load(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        res += current_load(machine_id);
    }
    return res;
}

double Probe::average_load(int machine_id){
    xbt_assert(context->load_used, "The load plugin has not been initialized");
    Machine * machine = context->machines[machine_id];
    double res = sg_host_get_avg_load(machine->host);
    return res;
}

double Probe::added_average_load(){
    double res = 0;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it)
    {   
        int machine_id = *it;
        res += average_load(machine_id);
    }
    return res;
}

/** Links Function */


double Probe::link_current_load(simgrid::s4u::Link* link){
    double res = link->get_usage();
    return res;
}

double Probe::link_average_load(simgrid::s4u::Link* link){
    double res = sg_link_get_avg_load(link);
    return res;
}

double Probe::link_consumed_energy(simgrid::s4u::Link* link){
    double res = sg_link_get_consumed_energy(link);
    return res;
}



double Probe::added_link_current_load(){
    double res =0;
    for(unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        res +=link_current_load(link);
    }
    return res;
}

double Probe::added_link_average_load(){
    double res =0;
    for(unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        res +=link_average_load(link);
    }
    return res;
}

double Probe::added_link_consumed_energy(){
    double res =0;
    for(unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        res +=link_consumed_energy(link);
    }
    return res;
}


double Probe::minimum_link_current_load(){
    double res = link_current_load(links[0]);
    for (unsigned i =1 ; i < links.size();i++){
        double intermediate = link_current_load(links[i]);
        if(intermediate < res){
            res = intermediate;
        }
    }
    return res;
}

double Probe::minimum_link_average_load(){
    double res = link_average_load(links[0]);
    for (unsigned i =1 ; i < links.size();i++){
        double intermediate = link_average_load(links[i]);
        if(intermediate < res){
            res = intermediate;
        }
    }
    return res;
}

double Probe::minimum_link_consumed_energy(){
    double res = link_consumed_energy(links[0]);
    for (unsigned i =1 ; i < links.size();i++){
        double intermediate = link_consumed_energy(links[i]);
        if(intermediate < res){
            res = intermediate;
        }
    }
    return res;
}



double Probe::maximum_link_current_load(){
    double res =0;
    for (unsigned i =0; i < links.size(); i++){
        double intermediate = link_current_load(links[i]);
        if(intermediate > res){
            res = intermediate;
        }
    }
    return res;
}


double Probe::maximum_link_average_load(){
    double res =0;
    for (unsigned i =0; i < links.size(); i++){
        double intermediate = link_average_load(links[i]);
        if(intermediate > res){
            res = intermediate;
        }
    }
    return res;
}

double Probe::maximum_link_consumed_energy(){
    double res =0;
    for (unsigned i =0; i < links.size(); i++){
        double intermediate = link_consumed_energy(links[i]);
        if(intermediate > res){
            res = intermediate;
        }
    }
    return res;
}




double Probe::average_link_current_load(){
    double res = added_link_current_load()/links.size();
    return res;
}

double Probe::average_link_average_load(){
    double res = added_link_average_load()/links.size();
    return res;
}

double Probe::average_link_consumed_energy(){
    double res = added_link_consumed_energy()/links.size();
    return res;
}


bool operator<(DetailedLinkData const &a, DetailedLinkData const &b){
    if(a.value < b.value){
        return true;
    }
    else {
        return false;
    }
}

double Probe::median_link_consumed_energy(){
    vector<DetailedLinkData> vec = link_detailed_consumed_energy();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}


double Probe::median_link_current_load(){
    vector<DetailedLinkData> vec = link_detailed_current_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_link_agg_load(){
    vector<DetailedLinkData> vec = link_detailed_average_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
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
        case Metrics::CURRENT_LOAD :
            res = average_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = average_agg_load();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = average_power_consumption();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_median(){
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = median_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = median_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = median_agg_load();
            break;
        case Metrics::POWER_CONSUMPTION :
            res = median_power_consumption();
            break;
        default:
            break;
    }
    return res;
}

double Probe::link_aggregate_addition(){ 
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = added_link_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = added_link_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = added_link_average_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::link_aggregate_maximum(){ 
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = maximum_link_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = maximum_link_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = maximum_link_average_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::link_aggregate_minimum(){ 
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = minimum_link_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = minimum_link_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = minimum_link_average_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::link_aggregate_average(){ 
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = average_link_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = average_link_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = average_link_average_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::link_aggregate_median(){
    double res = 0;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = median_link_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = median_link_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = median_link_agg_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_value(){
    double res =0;
    if(object == TypeOfObject::HOST){
    switch(aggregation){
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
        case TypeOfAggregation::MEDIAN :
            res = aggregate_median();
            break;
        default :
            break;
    }
    }
    else{
        switch(aggregation){
        case TypeOfAggregation::ADDITION :
            res = link_aggregate_addition();
            break;
        case TypeOfAggregation::MINIMUM :
            res = link_aggregate_minimum();
            break;
        case TypeOfAggregation::MAXIMUM :
            res = link_aggregate_maximum();
            break;
        case TypeOfAggregation::AVERAGE :
            res = link_aggregate_average();
            break;
        case TypeOfAggregation::MEDIAN :
            res = link_aggregate_median();
            break;
        default :
            break;
    }
    }
    return res;
}



vector<DetailedHostData> Probe::detailed_consumed_energy(){
    std::vector<DetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedHostData to_add;
        to_add.id= id;
        to_add.value = consumed_energy(id);
        res.push_back(to_add);
    }
    return res;
}

vector<DetailedHostData> Probe::detailed_power_consumption(){
    std::vector<DetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedHostData to_add;
        to_add.id= id;
        to_add.value = power_consumption(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<DetailedHostData> Probe::detailed_current_load(){
    std::vector<DetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedHostData to_add;
        to_add.id= id;
        to_add.value = current_load(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<DetailedHostData> Probe::detailed_average_load(){
    std::vector<DetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        DetailedHostData to_add;
        to_add.id= id;
        to_add.value = average_load(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<DetailedHostData> Probe::detailed_value(){
    std::vector<DetailedHostData> res;
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

vector<DetailedLinkData> Probe::link_detailed_consumed_energy(){
    vector<DetailedLinkData> res;
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        DetailedLinkData data;
        data.name = link->get_cname();
        data.value = link_consumed_energy(link);
        res.push_back(data);
    }
    return res;
}

vector<DetailedLinkData> Probe::link_detailed_current_load(){
    vector<DetailedLinkData> res;
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        DetailedLinkData data;
        data.name = link->get_cname();
        data.value = link_current_load(link);
        res.push_back(data);
    }
    return res;
}

vector<DetailedLinkData> Probe::link_detailed_average_load(){
    vector<DetailedLinkData> res;
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        DetailedLinkData data;
        data.name = link->get_cname();
        data.value = link_average_load(link);
        res.push_back(data);
    }
    return res;
}




vector<DetailedLinkData> Probe::link_detailed_value(){
    std::vector<DetailedLinkData> res;
    switch(metrics)
    {
        case Metrics::CONSUMED_ENERGY :
            res = link_detailed_consumed_energy();
            break;
        case Metrics::CURRENT_LOAD :
            res = link_detailed_current_load();
            break;
        case Metrics::AVERAGE_LOAD :
            res = link_detailed_average_load();
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
        case TypeOfAggregation::MEDIAN :
            res = "median";
            break;
        case TypeOfAggregation::NONE :
            res = "none";
            break;
        default :
            res = "unknown";
            break;
    }
    return res;
}






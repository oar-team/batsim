
#include "probe.hpp"

#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include <simgrid/host.h>
#include <simgrid/plugins/energy.h>
#include <simgrid/plugins/load.h>
#include <simgrid/s4u/Link.hpp>

#include "context.hpp"
#include "protocol.hpp"
#include "server.hpp"

using namespace std;
using namespace roles;

void verif_name(BatsimContext* context, std::string name){
    auto probes = context->probes;
    for (auto* probe : probes){
        if((probe->name).compare(name)==0){
            xbt_die("This name is already taken by another probe");
        }
    }
}



Probe* new_probe(IPMessage *task_data, ServerData* data){
    auto *message = static_cast<SchedAddProbeMessage *>(task_data->data);
    verif_name(data->context, message->name);

    Probe* nwprobe;
    nwprobe->context = data->context;
    nwprobe->name = message->name;
    nwprobe->object = message->object;
    nwprobe->metrics = message->metrics;
    nwprobe->trigger = message->trigger;
    nwprobe->aggregation = message->aggregation;

    vector<simgrid::s4u::Link*> links_to_add;
    std::vector<std::string> new_links_names;

    switch(nwprobe->object){
        case ProbeResourceType::LINK :
            new_links_names = message->links_names;
            for(unsigned i = 0 ; i < new_links_names.size();i++){
                simgrid::s4u::Link* link = simgrid::s4u::Link::by_name(new_links_names[i]);
                links_to_add.push_back(link);
    }
            nwprobe->links = links_to_add;
            break;
        case ProbeResourceType::HOST :  
            nwprobe->id_machines = message->machine_ids;
            break;
        default :
            break;
    }
    switch(nwprobe->trigger){
        case ProbeTriggerType::PERIODIC :
            nwprobe->period = message->period;
            nwprobe->nb_samples = message->nb_samples;
            break;
        default :
            break;
    }
    data->context->probes.push_back(nwprobe);
    return nwprobe;
}


void Probe::activation(){
    std::vector<simgrid::s4u::Host*> list;
    Machine * machine;
    if(object == ProbeResourceType::LINK){
                track_links();
            }
    switch(trigger){
        case ProbeTriggerType::ONE_SHOT :
            one_shot_reaction();
            break;
        case ProbeTriggerType::PERIODIC :
            machine = context->machines[0];
            simgrid::s4u::Actor::create("test", machine->host, periodic, this);
            // simgrid::s4u::Actor::create("test", machine->host, test_sleep, this);

            break;
        default :
            xbt_die("Unsupported or not yet implemented type of trigger");
            break;
    }
}

void Probe::destruction(){
    vector<Probe*> probes = context->probes;
    switch(object){
        case ProbeResourceType::LINK :
            untrack_links();
            break;
        default :
            for (unsigned i =0 ; i< probes.size(); i++){
                auto probe = probes[i];
                if((probe->name).compare(name)==0){
                    probes.erase(probes.begin()+i);
                    break;
                }
            }
            break;
    }
}

void Probe::track_links(){
    if(object != ProbeResourceType::LINK){
        xbt_die("This probe does not work on links");
    }
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        sg_link_load_track(link);
    }
}

void Probe::untrack_links(){
    if(object != ProbeResourceType::LINK){
        xbt_die("This probe does not work on links");
    }
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        sg_link_load_untrack(link);
    }
}

void Probe::one_shot_reaction(){
    auto *message = new ProbeDataMessage;
    message->probe_name = name;
    message->aggregation = aggregation;
    message->metrics = metrics;
    message->object = object;
    switch(aggregation){
        case ProbeAggregationType::NONE :
            switch(object){
                case ProbeResourceType::HOST :
                    message->vechd = detailed_value();
                    destruction();
                    break;
                case ProbeResourceType::LINK :
                    message->vecld = link_detailed_value();
                    destruction();
                    break;
                default :
                    xbt_die("Unsupported type of object");
                    break;
            }
            break;
        case ProbeAggregationType::UNKNOWN :
            xbt_die("Unsupported type of aggregation");
            break;
        default :
            message->value = aggregate_value();
            destruction();
            break;
    }
    //data->context->proto_writer->send_message_at_time(simgrid::s4u::Engine::get_clock(), "server", IPMessageType::PROBE_DATA, static_cast <void*>(message));
    //NO. private function.
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

bool operator<(ProbeDetailedHostData const &a, ProbeDetailedHostData const &b){
    if(a.value < b.value){
        return true;
    }
    else {
        return false;
    }
}

double Probe::median_consumed_energy(){
    vector<ProbeDetailedHostData> vec = detailed_consumed_energy();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_power_consumption(){
    vector<ProbeDetailedHostData> vec = detailed_power_consumption();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_current_load(){
    vector<ProbeDetailedHostData> vec = detailed_current_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_agg_load(){
    vector<ProbeDetailedHostData> vec = detailed_average_load();
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


bool operator<(ProbeDetailedLinkData const &a, ProbeDetailedLinkData const &b){
    if(a.value < b.value){
        return true;
    }
    else {
        return false;
    }
}

double Probe::median_link_consumed_energy(){
    vector<ProbeDetailedLinkData> vec = link_detailed_consumed_energy();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}


double Probe::median_link_current_load(){
    vector<ProbeDetailedLinkData> vec = link_detailed_current_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}

double Probe::median_link_agg_load(){
    vector<ProbeDetailedLinkData> vec = link_detailed_average_load();
    std::sort(vec.begin(), vec.end());
    int index = vec.size()/2;
    return (vec.at(index)).value;
}


double Probe::aggregate_addition(){ 
    double res = 0;
    switch(metrics)
    {
        case ProbeMetrics::CONSUMED_ENERGY :
            res = added_consumed_energy();
            break;
        case ProbeMetrics::POWER_CONSUMPTION :
            res = added_power_consumption();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = added_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = minimum_consumed_energy();
            break;
        case ProbeMetrics::POWER_CONSUMPTION :
            res = minimum_power_consumption();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = minimum_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = maximum_consumed_energy();
            break;
        case ProbeMetrics::POWER_CONSUMPTION :
            res = maximum_power_consumption();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = maximum_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = average_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = average_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
            res = average_agg_load();
            break;
        case ProbeMetrics::POWER_CONSUMPTION :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = median_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = median_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
            res = median_agg_load();
            break;
        case ProbeMetrics::POWER_CONSUMPTION :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = added_link_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = added_link_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = maximum_link_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = maximum_link_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = minimum_link_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = minimum_link_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = average_link_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = average_link_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
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
        case ProbeMetrics::CONSUMED_ENERGY :
            res = median_link_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = median_link_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
            res = median_link_agg_load();
            break;
        default:
            break;
    }
    return res;
}

double Probe::aggregate_value(){
    double res =0;
    if(object == ProbeResourceType::HOST){
    switch(aggregation){
        case ProbeAggregationType::ADDITION :
            res = aggregate_addition();
            break;
        case ProbeAggregationType::MINIMUM :
            res = aggregate_minimum();
            break;
        case ProbeAggregationType::MAXIMUM :
            res = aggregate_maximum();
            break;
        case ProbeAggregationType::AVERAGE :
            res = aggregate_average();
            break;
        case ProbeAggregationType::MEDIAN :
            res = aggregate_median();
            break;
        default :
            break;
    }
    }
    else{
        switch(aggregation){
        case ProbeAggregationType::ADDITION :
            res = link_aggregate_addition();
            break;
        case ProbeAggregationType::MINIMUM :
            res = link_aggregate_minimum();
            break;
        case ProbeAggregationType::MAXIMUM :
            res = link_aggregate_maximum();
            break;
        case ProbeAggregationType::AVERAGE :
            res = link_aggregate_average();
            break;
        case ProbeAggregationType::MEDIAN :
            res = link_aggregate_median();
            break;
        default :
            break;
    }
    }
    return res;
}



vector<ProbeDetailedHostData> Probe::detailed_consumed_energy(){
    std::vector<ProbeDetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        ProbeDetailedHostData to_add;
        to_add.id= id;
        to_add.value = consumed_energy(id);
        res.push_back(to_add);
    }
    return res;
}

vector<ProbeDetailedHostData> Probe::detailed_power_consumption(){
    std::vector<ProbeDetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        ProbeDetailedHostData to_add;
        to_add.id= id;
        to_add.value = power_consumption(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<ProbeDetailedHostData> Probe::detailed_current_load(){
    std::vector<ProbeDetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        ProbeDetailedHostData to_add;
        to_add.id= id;
        to_add.value = current_load(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<ProbeDetailedHostData> Probe::detailed_average_load(){
    std::vector<ProbeDetailedHostData> res;
    for (auto it = id_machines.elements_begin(); it != id_machines.elements_end(); ++it){
        int id = *it;
        ProbeDetailedHostData to_add;
        to_add.id= id;
        to_add.value = average_load(id);
        res.push_back(to_add);
    }
    return res;    
}

vector<ProbeDetailedHostData> Probe::detailed_value(){
    std::vector<ProbeDetailedHostData> res;
    switch(metrics)
    {
        case ProbeMetrics::CONSUMED_ENERGY :
            res = detailed_consumed_energy();
            break;
        case ProbeMetrics::POWER_CONSUMPTION :
            res = detailed_power_consumption();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = detailed_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
            res = detailed_average_load();
            break;
        default:
            break;
    }
    return res;
}

vector<ProbeDetailedLinkData> Probe::link_detailed_consumed_energy(){
    vector<ProbeDetailedLinkData> res;
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        ProbeDetailedLinkData data;
        data.name = link->get_cname();
        data.value = link_consumed_energy(link);
        res.push_back(data);
    }
    return res;
}

vector<ProbeDetailedLinkData> Probe::link_detailed_current_load(){
    vector<ProbeDetailedLinkData> res;
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        ProbeDetailedLinkData data;
        data.name = link->get_cname();
        data.value = link_current_load(link);
        res.push_back(data);
    }
    return res;
}

vector<ProbeDetailedLinkData> Probe::link_detailed_average_load(){
    vector<ProbeDetailedLinkData> res;
    for (unsigned i =0 ; i < links.size(); i++){
        simgrid::s4u::Link* link = links[i];
        ProbeDetailedLinkData data;
        data.name = link->get_cname();
        data.value = link_average_load(link);
        res.push_back(data);
    }
    return res;
}




vector<ProbeDetailedLinkData> Probe::link_detailed_value(){
    std::vector<ProbeDetailedLinkData> res;
    switch(metrics)
    {
        case ProbeMetrics::CONSUMED_ENERGY :
            res = link_detailed_consumed_energy();
            break;
        case ProbeMetrics::CURRENT_LOAD :
            res = link_detailed_current_load();
            break;
        case ProbeMetrics::AVERAGE_LOAD :
            res = link_detailed_average_load();
            break;
        default:
            break;
    }
    return res;
}

std::string  aggregation_to_string(ProbeAggregationType type){
    std::string res ="";
    switch(type){
        case ProbeAggregationType::ADDITION :
            res = "addition";
            break;
        case ProbeAggregationType::MINIMUM :
            res = "minimum";
            break;
        case ProbeAggregationType::MAXIMUM :
            res = "maximum";
            break;
        case ProbeAggregationType::AVERAGE :
            res = "average";
            break;
        case ProbeAggregationType::MEDIAN :
            res = "median";
            break;
        case ProbeAggregationType::NONE :
            res = "none";
            break;
        default :
            res = "unknown";
            break;
    }
    return res;
}


/** Periodic probe*/

void periodic(Probe* probe){
    vector<ProbeDetailedHostData> host_value; 
    double value;
    for (int i =0 ; i< probe->nb_samples ;i ++){
        switch(probe->aggregation){
            case ProbeAggregationType::NONE : 
                host_value = probe->detailed_value();
                probe->context->proto_writer->append_detailed_probe_data(probe->name,simgrid::s4u::Engine::get_clock(),host_value,probe->metrics);
                break;
            default :
                value = probe->aggregate_value();
                probe->context->proto_writer->append_aggregate_probe_data(probe->name,simgrid::s4u::Engine::get_clock(),value,probe->aggregation,probe->metrics);
                break;
        }
        simgrid::s4u::this_actor::sleep_for(probe->period);
    }
    probe->destruction();
}

void test_sleep(Probe* probe){ 
    vector<ProbeDetailedHostData> host_value; 
    double value;
    int nb = probe->nb_samples;
    for (int i =0 ; i< nb ; i++){
    switch(probe->aggregation){
            case ProbeAggregationType::NONE : 
                host_value = probe->detailed_value();
                probe->context->proto_writer->append_detailed_probe_data(probe->name,simgrid::s4u::Engine::get_clock(),host_value,probe->metrics);
                break;
            default :
                value = probe->aggregate_value();
                probe->context->proto_writer->append_aggregate_probe_data(probe->name,simgrid::s4u::Engine::get_clock(),value,probe->aggregation,probe->metrics);
                break;
        }
    }

    for (int j =0 ; j < nb; j++){
        simgrid::s4u::this_actor::sleep_for(probe->period);
}
    
}





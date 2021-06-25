

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <simgrid/s4u/Link.hpp>
#include "machines.hpp"


struct BatsimContext;

enum class Metrics
{
    CONSUMED_ENERGY,
    POWER_CONSUMPTION,
    CURRENT_LOAD,
    AVERAGE_LOAD,
    UNKNOWN
};

enum class TypeOfAggregation
{
    ADDITION,
    AVERAGE,
    MEDIAN,
    MINIMUM,
    MAXIMUM,
    NONE,
    UNKNOWN
};

enum class TypeOfObject
{
    HOST,
    LINK,
    UNKNOWN
};

enum class TypeOfTrigger
{
    ONE_SHOT,
    UNKNOWN
};

struct DetailedHostData{
    int id;
    double value;
};

struct DetailedLinkData{
    std::string name; 
    double value;
    
};

    /**
     * @brief Returns true if the value field of a is less than the value field of b
     */

    bool operator<(DetailedHostData const &a, DetailedHostData const &b);

    bool operator<(DetailedLinkData const &a, DetailedLinkData const &b);





struct Probe{

    std::string name;
    TypeOfObject object;
    TypeOfAggregation aggregation;
    Metrics metrics;
    IntervalSet id_machines;
    BatsimContext * context;
    std::vector<simgrid::s4u::Link*> links;
    TypeOfTrigger trigger;

    /**
     * @brief Active the probe and track links which have to be tracked.
     */

    void activation();

    /**
     * @brief Destroy the probe and take it off from the probes vector of the batsim context, also untrack links if it was a link probe.
     */

    void destruction();

    /**
     * @brief The reaction of our probe if the type of trigger is one shot.
     */

    void one_shot_reaction();


    /**
     * @brief Tracks all links wich have to be track.
     */

    void track_links();

    /**
     * @brief Untracks links wich have been tracked by our probe.
     */

    void untrack_links();

    /**
     * @brief This function verifies if the name is not already used by anoter probe, if the name is already taken, it stops batsim
     */

    void verif_name(BatsimContext* context, std::string name);

    /**
     * @brief Returns the average of the consumed energy
     */
    double average_consumed_energy();

    /**
     * @brief Returns the average of the power consumption
     */
    double average_power_consumption();

    /**
     * @brief Returns the average of the current load
     */
    double average_current_load();

    /**
     * @brief Returns the average of the load
     */
    double average_agg_load();

    /**
     * @brief Returns the median of consumed energy
     */

    double median_consumed_energy();

    double median_power_consumption();

    double median_current_load();

    double median_agg_load();

    /**
     * @brief Returns the minimum of the consumed energy
     */
    double minimum_consumed_energy();

    /**
     * @brief Returns the minimum of the power consumption
     */
    double minimum_power_consumption();

    /**
     * @brief Returns the minimum of the current load
     */
    double minimum_current_load();

    /**
     * @brief Returns the minimum of the load
     */
    double minimum_agg_load();

    /**
     * @brief Returns the maximum of the consumed energy
     */
    double maximum_consumed_energy();

    /**
     * @brief Returns the maximum of the power consumption
     */
    double maximum_power_consumption();

    /**
     * @brief Returns the maximum of the current load
     */
    double maximum_current_load();

    /**
     * @brief Returns the maximum of the load
     */
    double maximum_agg_load();

    /**
     * @brief Returns the vale of the consumed energy of machines which are contains in the id_marchiens IntervalSet
     */
     double added_consumed_energy();


    /** 
     * @brief Returns the power_consumption of machines which are studied by our probe
     */

    double added_power_consumption();

    /**
     * @brief Returns the current_load of machines which are studied by our probe_name
     */

    double added_current_load();

    /**
     * @brief Returns the load average of machines which are studied by our probe_name
     */

    double added_average_load();

    /**
     * @brief Returns the vale of the consumed energy of the machine that is described by its id 
     */
     double consumed_energy(int id);


    /** 
     * @brief Returns the power_consumption of the machine that is described by its id 
     */

    double power_consumption(int id);

    /**
     * @brief Returns the current_load of the machine that is described by its id 
     */

    double current_load(int id);

    /**
     * @brief Returns the load average of the machine that is described by its id 
     */

    double average_load(int id);


    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and consumed energy associated
     */

    std::vector<DetailedHostData> detailed_consumed_energy();

    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and the power consumption associated
     */

   std::vector<DetailedHostData> detailed_power_consumption();

    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and the average load associated
     */

    std::vector<DetailedHostData> detailed_average_load();

    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and the current load associated
     */

    std::vector<DetailedHostData> detailed_current_load();


    /**
     * @brief Returns a vector of value which has been measured
     */
    std::vector<DetailedHostData> detailed_value();

    /**
     * @brief Returns the aggregate added value according to the type of the Metrics
     */

    double aggregate_addition();

    /**
     * @brief Returns the aggregate minimum value according to the type of the Metrics
     */

    double aggregate_minimum();

    /**
     * @brief Returns the aggregate maximum value according to the type of the Metrics
     */

    double aggregate_maximum();

    /**
     * @brief Returns the aggregate averaged value according to the type of the Metrics
     */

    double aggregate_average();

    /**
     * @brief Returns the aggregate median value according to the type of the Metrics
     */

    double aggregate_median();

    /**
     * @brief Returns the aggregate value
     */
    
    double aggregate_value();

    /**
     * @brief Returns the link detailed value
     */

    std::vector<DetailedLinkData> link_detailed_value();


    /**
     * @brief Returns the current load of the link passed in parameter
     */

    double link_current_load(simgrid::s4u::Link* link);

    /**
     * @brief Returns the average load of the link passed in parameter
     */

    double link_average_load(simgrid::s4u::Link* link);

    /**
     * @brief Returns the consumed energy of the link passed in parameter
     */

    double link_consumed_energy(simgrid::s4u::Link* link);


    /**
     * @brief Returns the link added aggregate value
     */

    double link_aggregate_addition();

    /**
     * @brief Returns the link minimum value
     */
           
    double link_aggregate_minimum();

    /**
     * @brief Returns the link maximum value
     */
           
    double link_aggregate_maximum();

    /**
     * @brief Returns the averaged link aggregate value
     */
    
    double link_aggregate_average();

    /**
     * @brief Returns the median link value depending on the  Metrics
     */

    double link_aggregate_median();

    /**
     * @brief Returns the median link consumed energy
     */

    double median_link_consumed_energy();

    /**
     * @brief Returns the median link current load
     */

    double median_link_current_load();

    /**
     * @brief Returns the average link average load
     */

    double median_link_agg_load();

    /**
     * @brief Returns the added link current load
     */

    double added_link_current_load();

    /**
     * @brief Returns the added link average load
     */ 

    double added_link_average_load();

    /**
     * @brief Returns the added link consumed energy
     */ 

    double added_link_consumed_energy();


    /**
     * @brief Returns the minimum link current load
     */ 

    double minimum_link_current_load();

    /**
     * @brief Returns the minimum link average load
     */ 

    double minimum_link_average_load();

    /**
     * @brief Returns the minimum link consumed energy
     */ 

    double minimum_link_consumed_energy();


    /**
     * @brief Return the maximum link current load
     */ 

    double maximum_link_current_load();

    /**
     * @brief Returns the maximum link average load
     */ 

    double maximum_link_average_load();

    /**
     * @brief Returns the maximum link consumed energy
     */ 

    double maximum_link_consumed_energy();
    

    /**
     * @brief Returns the averaged link current load
     */ 

    double average_link_current_load();
    
    /**
     * @brief Returns the averaged link average load
     */ 

    double average_link_average_load();

    /**
     * @brief Returns the averaged link consumed energy
     */ 

    double average_link_consumed_energy();

   
    /**
     * @brief Returns the details of the link consumed energy
     */ 

    std::vector<DetailedLinkData> link_detailed_consumed_energy();

    /**
     * @brief Returns the details of the link current load
     */ 

    std::vector<DetailedLinkData> link_detailed_current_load();

    /**
     * @brief Returns the details of the link average load
     */ 

    std::vector<DetailedLinkData> link_detailed_average_load();
};

/**
 * @brief Returns a string corresponding to the TypeOfAggregation field of
 */

    std::string aggregation_to_string(TypeOfAggregation type);

    /**
     * @brief returns a new host probe.
     * @param[in] name the name of the new probe.
     * @param[in] met the metrics that we want to study
     * @param[in] agg the type of aggregation that we want, NONE if we don't want to aggregate
     * @param[in] machines the IntervalSet of machines that our probe will work on.
     * @param[in] context the batsim context.
     */ 
    Probe new_host_probe(std::string name, TypeOfTrigger trigger, Metrics met, TypeOfAggregation agg,const IntervalSet & machines, BatsimContext * context);

    /**
     * @brief returns a new link probe 
     * @param[in] name the name of the new probe 
     * @param[in] met the metrics that we want to study
     * @param[in] agg the type of aggregation that we want, NONE if we don't want to aggregate
     * @param[in] context the batsim context.
     */
    Probe new_link_probe(std::string name, TypeOfTrigger trigger, Metrics met, TypeOfAggregation agg, std::vector<std::string> links_name, BatsimContext * context);
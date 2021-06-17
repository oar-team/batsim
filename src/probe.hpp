

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "machines.hpp"


struct BatsimContext;
class Probes;

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
    MINIMUM,
    MAXIMUM,
    NONE,
    UNKNOWN
};

struct DetailedData{
    int id;
    double value;
};


struct Probe{

    std::string name;
    TypeOfAggregation aggregation;
    Metrics metrics;
    IntervalSet id_machines;
    BatsimContext * context;



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

    std::vector<DetailedData> detailed_consumed_energy();

    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and the power consumption associated
     */

   std::vector<DetailedData> detailed_power_consumption();

    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and the average load associated
     */

    std::vector<DetailedData> detailed_average_load();

    /**
     * @brief Returns a vector composed of NonAggrData wich corresponds to machine id and the current load associated
     */

    std::vector<DetailedData> detailed_current_load();


    /**
     * @brief Returns a vector of value which has been measured
     */
    std::vector<DetailedData> detailed_value();

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
     * @brief Returns the aggregate value
     */
    
    double aggregate_value();
};

/**
 * @brief Returns a string corresponding to the TypeOfAggregation field of
 */

    std::string aggregation_to_string(TypeOfAggregation type);

/**
     * @brief returns a new probe.
     * @param[in] name the name of the new probe 
     * @param[in] machines the IntervalSet of machines that our probe will work on.
     */ 
    Probe new_probe(std::string name, Metrics met, TypeOfAggregation agg, const IntervalSet & machines, BatsimContext * context);

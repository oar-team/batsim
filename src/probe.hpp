

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "machines.hpp"


struct BatsimContext;
class Probes;

enum class Metric
{
    CONSUMED_ENERGY,
    POWER_CONSUMPTION,
    CURRENT_LOAD,
    AVERAGE_LOAD,
    UNKNOWN
};


struct Probe{

    std::string name;
    Metric metrics;
    IntervalSet id_machines;
    BatsimContext * context;

    /**
     * @brief Returns a value depending on the metrics.
     */ 
    double return_value();

    /**
     * @brief Returns the vale of the consumed energy of machines which are contains in the id_marchiens IntervalSet
     */
     double consumed_energy();


    /** 
     * @brief Returns the power_consumption of machines which are studied by our probe
     */

    double power_consumption();

    /**
     * @brief Returns the current_load of machines which are studied by our probe_name
     */

    double current_load();

    /**
     * @brief Returns the load average of machines which are studied by our probe_name
     */

    double average_load();

};

/**
     * @brief returns a new probe.
     * @param[in] name the name of the new probe 
     * @param[in] machines the IntervalSet of machines that our probe will work on.
     */ 
    Probe new_probe(std::string name,const IntervalSet & machines, BatsimContext * context);

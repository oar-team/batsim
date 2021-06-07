

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "machines.hpp"


struct BatsimContext;
class Probes;


struct Probe{

    std::string name;
    std::string metrics;
    IntervalSet id_machines;
    BatsimContext * context;

    /**
     * @brief Returns a value depending on the metrics.
     */ 
    double return_value();

    /**
     * @brief Returns the vale of the consumed energy of machines which are contains in the id_marchiens IntervalSet
     */
     double get_consumed_energy();


    /** 
     * @brief Returns the power_consumption of machines which are study by our probe
     */

    double power_consumption();

};

/**
     * @brief returns a new probe.
     * @param[in] name the name of the new probe 
     * @param[in] machines the IntervalSet of machines that our probe will work on.
     */ 
    Probe new_probe(std::string name,const IntervalSet & machines, BatsimContext * context);

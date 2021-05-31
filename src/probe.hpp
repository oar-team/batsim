

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
    IntervalSet id_machines;

    /**
     * @brief Returns the vale of the consumed energy of machines which are contains in the id_marchiens IntervalSet
     */
    long double get_consumed_energy(BatsimContext * context);


    /** 
     * @brief Returns the power_consumption of machines which are study by our probe
     */

    double power_consumption(BatsimContext * context);

};

/**
     * @brief returns a new probe.
     * @param[in] name the name of the new probe 
     * @param[in] machines the IntervalSet of machines that our probe will work on.
     */ 
    Probe new_probe(std::string name,const IntervalSet & machines);

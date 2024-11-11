/**
 * @file periodic.hpp
 * @brief This module is in charge of managing periodic events requested by the EDC
 */

#pragma once

#include <string>
#include <vector>
#include <map>

#include "ipp.hpp"

struct BatsimContext;

struct TimeSlice
{
  std::vector<CallMeLaterMessage*> cml_triggers;
  std::vector<CreateProbeMessage*> probes;
  double duration;
};

void generate_static_periodic_schedule(
  const std::map<std::string, CallMeLaterMessage*> & cml_triggers,
  const std::map<std::string, CreateProbeMessage*> & probes,
  std::vector<TimeSlice> & schedule,
  uint64_t & slice_duration
);

void periodic_main_actor(BatsimContext * context);

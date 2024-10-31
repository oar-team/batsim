#pragma once

#include <string>
#include <vector>

enum PeriodicTriggerType
{
  CALL_ME_LATER
, PROBE
};

struct CallMeLaterPeriodicTrigger
{
  std::string id;
};

struct PeriodicTrigger
{
  bool is_finite;
  int nb_remaining_triggers;
  PeriodicTriggerType type;
  void * data;
};

struct TimeSlice
{
  std::vector<PeriodicTrigger *> triggers;
  float duration;
};

void generate_static_periodic_schedule(const std::vector<PeriodicTrigger*> & triggers, std::vector<TimeSlice> & schedule);

void periodic_main_actor();

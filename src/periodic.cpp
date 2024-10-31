/**
 * @file periodic.cpp
 * @brief This module is in charge of managing periodic events requested by the EDC
 */
#include "periodic.hpp"

#include <cmath>
#include <set>

#include <simgrid/s4u.hpp>

#include "ipp.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(periodic, "periodic"); //!< Logging

enum class PeriodicTriggerType
{
  CALL_ME_LATER
, PROBE
};

const char * periodic_trigger_type_to_cstring(PeriodicTriggerType type) {
  switch (type) {
    case PeriodicTriggerType::CALL_ME_LATER:
      return "CALL_ME_LATER";
    case PeriodicTriggerType::PROBE:
      return "PROBE";
  };
  return nullptr;
}

struct PeriodicTrigger
{
  Periodic periodic;
  PeriodicTriggerType type;
  void * data;
};

const char * periodic_trigger_id_cstring(const PeriodicTrigger & trigger) {
  switch (trigger.type) {
    case PeriodicTriggerType::CALL_ME_LATER:
      return (static_cast<CallMeLaterMessage*>(trigger.data))->call_id.c_str();
    case PeriodicTriggerType::PROBE:
      xbt_assert(false, "TODO: implement me");
  }
  return nullptr;
}

void set_periodic_in_ms(Periodic & p) {
  switch (p.time_unit) {
    case batprotocol::fb::TimeUnit_Millisecond:
      return;
    case batprotocol::fb::TimeUnit_Second: {
      p.time_unit = batprotocol::fb::TimeUnit_Millisecond;
      p.period *= 1000;
    } break;
  }
}

void generate_static_periodic_schedule(
  const std::map<std::string, CallMeLaterMessage*> & cml_triggers,
  std::vector<TimeSlice> & schedule,
  uint64_t & slice_duration
) {
  // check that all periods have offset=0 and are expressed in the same unit (milliseconds)
  std::map<uint64_t, std::vector<PeriodicTrigger>> triggers;
  for (const auto & [_, cml] : cml_triggers) {
    xbt_assert(cml->periodic.offset == 0, "CallMeLater (id='%s') has non-zero offset (%lu), which is not supported", cml->call_id.c_str(), cml->periodic.offset);
    xbt_assert(cml->periodic.time_unit == batprotocol::fb::TimeUnit_Millisecond, "internal inconsistency: CallMeLater (id='%s') has non-ms time units, which should not happen here", cml->call_id.c_str());

    auto trigger = PeriodicTrigger{
      cml->periodic,
      PeriodicTriggerType::CALL_ME_LATER,
      (void*)cml
    };

    auto it = triggers.find(cml->periodic.period);
    if (it == triggers.end())
      triggers[cml->periodic.period] = {trigger};
    else
      it->second.emplace_back(trigger);
  }

  // check that all different periods are multiple or each other
  bool period_inconsistency = false;
  std::vector<uint64_t> periods;
  for (const auto& [period, _] : triggers)
    periods.push_back(period);
  for (unsigned int i = 0; i < periods.size(); ++i) {
    for (unsigned int j = i+1; j < periods.size(); ++j) {
      if (periods[j] % periods[i] != 0) {
        period_inconsistency = true;
        const auto & p_i = triggers[periods[i]].at(0);
        const auto & p_j = triggers[periods[j]].at(0);
        XBT_ERROR("Period inconsistency found within periodic triggers."
          "All periods should be the same, or longer periods should be a multiple of all shorter periods."
          "Period %lu is incompatible with period %lu, as the remainder of %lu/%lu is non-zero."
          "Example of triggers with such periods:"
          "%s(id='%s', period=%lu ms) and %s(id='%s', period=%lu ms)",
          periods[j], periods[i], periods[j], periods[i],
          periodic_trigger_type_to_cstring(p_j.type), periodic_trigger_id_cstring(p_j), p_j.periodic.period,
          periodic_trigger_type_to_cstring(p_i.type), periodic_trigger_id_cstring(p_i), p_i.periodic.period
        );
      }
    }
  }
  xbt_assert(!period_inconsistency, "Period inconsistency found, aborting");

  // clear output values and leave now in degenerate case (nothing to do)
  schedule.clear();
  slice_duration = 0;
  if (triggers.empty())
    return;

  // compute how many slices are needed
  std::vector<uint64_t> multiples;
  uint64_t nb_slices = 1;
  for (unsigned int i = 1; i < periods.size(); ++i) {
    uint64_t multiple = periods[i] / periods[i-1];
    multiples.push_back(multiple);
    nb_slices *= multiple;
  }

  // populate static schedule, while checking for integer overflows
  schedule.resize(nb_slices);
  slice_duration = periods.at(0);
  uint64_t previous_time = 0;
  for (unsigned int i = 0; i < nb_slices; ++i) {
    uint64_t current_time = slice_duration * i;
    xbt_assert(current_time >= previous_time, "integer overflow?!");
    auto & slice = schedule[i];
    slice.duration = (double)slice_duration;
    slice.cml_triggers.clear();
    for (const auto & [_, cml] : cml_triggers) {
      if (current_time % cml->periodic.period == 0)
        slice.cml_triggers.push_back(cml);
    }
    previous_time = current_time;
  }
}

void periodic_main_actor()
{
  auto mbox = simgrid::s4u::Mailbox::by_name("periodic");
  bool need_reschedule = false;
  bool die_received = false;
  std::map<std::string, CallMeLaterMessage*> cml_triggers;

  std::vector<TimeSlice> static_schedule;
  uint64_t slice_duration = 0;
  unsigned int current_slice_i = 0;
  double next_timeout_duration = 0;

  while (!die_received) {
    need_reschedule = false;
    // Wait for current slice to terminate while being able to receive a message from the server.
    // If there is currently no triggers, just wait for a message without timeout.
    IPMessage * message = nullptr;
    try {
      if (static_schedule.empty())
        message = mbox->get<IPMessage>();
      else {
        uint64_t total_schedule_length = slice_duration * static_schedule.size();
        double current_time = simgrid::s4u::Engine::get_clock() * 1e3; // simgrid is in s, this module is in ms
        double current_time_mod_schedule_length = fmod(current_time, (double)total_schedule_length);
        double elapsed = fmod(current_time_mod_schedule_length, slice_duration);
        current_slice_i = (uint64_t)(current_time_mod_schedule_length) / slice_duration;
        next_timeout_duration = static_schedule[current_slice_i].duration - elapsed;
        xbt_assert(next_timeout_duration > 0, "internal inconsistency");
        message = mbox->get<IPMessage>(next_timeout_duration * 1e-3); // this modules expresses everything in milliseconds
      }

      // A message from the server has been received
      switch(message->type) {
        case IPMessageType::DIE: {
          die_received = true;
        } break;
        case IPMessageType::SCHED_CALL_ME_LATER: {
          auto msg = static_cast<CallMeLaterMessage*>(message->data);
          message->data = nullptr;
          auto it = cml_triggers.find(msg->call_id);
          xbt_assert(it == cml_triggers.end(), "received a new CallMeLater with call_id='%s' while this call_id is already in use", msg->call_id.c_str());
          xbt_assert(msg->periodic.is_infinite == false, "only non-infinite triggers are implemented for now");
          xbt_assert(msg->periodic.nb_periods >= 1, "invalid CallMeLater (call_id='%s'): finite but nb_periods=%u", msg->call_id.c_str(), msg->periodic.nb_periods);
          set_periodic_in_ms(msg->periodic);
          cml_triggers[msg->call_id] = msg;
          need_reschedule = true;
        } break;
        default: {
          xbt_assert(false, "Unexpected message received: %s", ip_message_type_to_string(message->type).c_str());
        } break;
      }
      delete message;
      message = nullptr;

      if (die_received)
        break;
    }
    catch (const simgrid::TimeoutException&) {
      // The end of the current slice has been reached without receiving any message from the server
      current_slice_i = (current_slice_i + 1) % static_schedule.size();

      // Populate the content of the full message that should be sent to the server
      auto * msg = new PeriodicTriggerMessage;
      auto & slice = static_schedule[current_slice_i];
      for (auto * cml : slice.cml_triggers) {
        msg->calls.emplace_back(RequestedCall{
          cml->call_id,
          !cml->periodic.is_infinite && cml->periodic.nb_periods == 1
        });

        if (!cml->periodic.is_infinite) {
          --cml->periodic.nb_periods;
          if (cml->periodic.nb_periods == 0) {
            XBT_INFO("Periodic trigger CallMeLater(call_id='%s') just issued its last call!", cml->call_id.c_str());
            need_reschedule = true;
            cml_triggers.erase(cml->call_id);
            delete cml;
          }
        }
      }

      send_message("server", IPMessageType::PERIODIC_TRIGGER, static_cast<void*>(msg));
    }

    if (need_reschedule) {
      generate_static_periodic_schedule(cml_triggers, static_schedule, slice_duration);
    }
  }
}

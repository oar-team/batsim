/**
 * @file periodic.cpp
 * @brief This module is in charge of managing periodic events requested by the EDC
 */
#include "periodic.hpp"

#include <cmath>
#include <set>

#include <simgrid/s4u.hpp>
#include <simgrid/plugins/energy.h>

#include "context.hpp"
#include "ipp.hpp"
#include "machines.hpp"

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
  const std::map<std::string, CreateProbeMessage*> & probes,
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

  for (const auto & [_, probe] : probes) {
    xbt_assert(probe->periodic.offset == 0, "Probe (id='%s') has non-zero offset (%lu), which is not supported", probe->probe_id.c_str(), probe->periodic.offset);
    xbt_assert(probe->periodic.time_unit == batprotocol::fb::TimeUnit_Millisecond, "internal inconsistency: Probe (id='%s') has non-ms time units, which should not happen here", probe->probe_id.c_str());

    auto trigger = PeriodicTrigger{
      probe->periodic,
      PeriodicTriggerType::PROBE,
      (void*)probe
    };

    auto it = triggers.find(probe->periodic.period);
    if (it == triggers.end())
      triggers[probe->periodic.period] = {trigger};
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
    for (const auto & [_, probe] : probes) {
      if (current_time % probe->periodic.period == 0)
        slice.probes.push_back(probe);
    }
    previous_time = current_time;
  }
}

void periodic_main_actor(BatsimContext * context)
{
  auto mbox = simgrid::s4u::Mailbox::by_name("periodic");
  bool need_reschedule = false;
  bool die_received = false;
  std::map<std::string, CallMeLaterMessage*> cml_triggers;
  std::map<std::string, CreateProbeMessage*> probes;

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
          xbt_assert(msg->periodic.is_infinite || msg->periodic.nb_periods >= 1, "invalid CallMeLater (call_id='%s'): finite but nb_periods=%u should be greater than 0", msg->call_id.c_str(), msg->periodic.nb_periods);
          set_periodic_in_ms(msg->periodic);
          cml_triggers[msg->call_id] = msg;
          need_reschedule = true;
        } break;
        case IPMessageType::SCHED_CREATE_PROBE: {
          auto msg = static_cast<CreateProbeMessage*>(message->data);
          message->data = nullptr;
          msg->initialized = false;
          auto it = probes.find(msg->probe_id);
          xbt_assert(it == probes.end(), "received a new CreateProbe with probe_id='%s' while this probe_id is already in use", msg->probe_id.c_str());
          xbt_assert(msg->periodic.is_infinite || msg->periodic.nb_periods >= 1, "invalid CreateProbe (probe_id='%s'): finite but nb_periods=%u should be greater than 0", msg->probe_id.c_str(), msg->periodic.nb_periods);
          set_periodic_in_ms(msg->periodic);
          probes[msg->probe_id] = msg;
          need_reschedule = true;
        } break;
        case IPMessageType::SCHED_STOP_CALL_ME_LATER: {
          auto msg = static_cast<StopCallMeLaterMessage*>(message->data);
          message->data = nullptr;
          auto it = cml_triggers.find(msg->call_id);
          if (it == cml_triggers.end()) {
            XBT_WARN("Received a StopCallMeLater on call_id='%s', but no such call is running", msg->call_id.c_str());
          } else {
            XBT_INFO("Stopping CallMeLater on call_id='%s'", msg->call_id.c_str());

            auto * m = new PeriodicEntityStoppedMessage;
            m->entity_id = msg->call_id;
            m->is_probe = false;
            m->is_call_me_later = true;
            send_message("server", IPMessageType::PERIODIC_ENTITY_STOPPED, static_cast<void*>(m));

            cml_triggers.erase(msg->call_id);
            delete msg;
            need_reschedule = true;
          }
        } break;
        case IPMessageType::SCHED_STOP_PROBE: {
          auto msg = static_cast<StopProbeMessage*>(message->data);
          message->data = nullptr;
          auto it = probes.find(msg->probe_id);
          if (it == probes.end()) {
            XBT_WARN("Received a StopProbe on probe_id='%s', but no such probe is running", msg->probe_id.c_str());
          } else {
            XBT_INFO("Stopping probe with probe_id='%s'", msg->probe_id.c_str());

            auto * m = new PeriodicEntityStoppedMessage;
            m->entity_id = msg->probe_id;
            m->is_probe = true;
            m->is_call_me_later = false;
            send_message("server", IPMessageType::PERIODIC_ENTITY_STOPPED, static_cast<void*>(m));

            probes.erase(msg->probe_id);
            delete msg;
            need_reschedule = true;
          }
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

      // CallMeLater triggers
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

      // Probe data
      for (auto probe_it = slice.probes.begin(); probe_it != slice.probes.end(); ) {
        auto * probe = *probe_it;
        bool erased = false;
        if (probe->initialized) {
          xbt_assert(probe->metrics == batprotocol::fb::Metrics_Power, "only the power metrics is implemented");
          xbt_assert(probe->resource_type == batprotocol::fb::Resources_HostResources, "only the host resource type is implemented");
          xbt_assert(context->energy_used, "trying to probe energy on hosts but the 'host_energy' SimGrid plugin has not been enabled");

          // TODO: populate the immutable fields only once, then just memcpy this for a new probe? or just put the original probe data into the message? (requires smart ptr)
          auto * probe_data = new ProbeData;
          probe_data->probe_id = probe->probe_id;
          probe_data->resource_type = probe->resource_type;
          probe_data->hosts = probe->hosts;
          probe_data->metrics = probe->metrics;

          probe_data->manually_triggered = false;
          probe_data->nb_triggered = 0; // TODO: implement me
          probe_data->nb_emitted = 0; // TODO: implement me
          probe_data->is_last_periodic = !probe->periodic.is_infinite && probe->periodic.nb_periods == 1;

          // TODO: populate an iterator of s4u hosts once per probe to avoid this slow traversal + host retrieval
          probe_data->vectorial_data.reserve(probe->hosts.size());
          for (auto it = probe->hosts.elements_begin(); it != probe->hosts.elements_end(); ++it)
          {
              int machine_id = *it;
              Machine * machine = context->machines[machine_id];
              probe_data->vectorial_data.emplace_back(sg_host_get_consumed_energy(machine->host));
          }

          switch(probe->resource_agregation_type) {
            case batprotocol::fb::ResourcesAggregationFunction_NoResourcesAggregation: {
              probe_data->data_type = batprotocol::fb::ProbeData_VectorialProbeData;
            } break;
            case batprotocol::fb::ResourcesAggregationFunction_Sum:
            case batprotocol::fb::ResourcesAggregationFunction_ArithmeticMean: {
              probe_data->data_type = batprotocol::fb::ProbeData_AggregatedProbeData;
              probe_data->aggregated_data = 0.0;
              for (const auto & value : probe_data->vectorial_data)
                probe_data->aggregated_data += value;
              if (probe->resource_agregation_type == batprotocol::fb::ResourcesAggregationFunction_ArithmeticMean)
                probe_data->aggregated_data /= probe->hosts.size();
            } break;
            default: {
              xbt_assert(false, "unimplemented resource aggregation type");
            } break;
          };

          msg->probes_data.emplace_back(probe_data);

          if (!probe->periodic.is_infinite) {
            --probe->periodic.nb_periods;
            if (probe->periodic.nb_periods == 0) {
              XBT_INFO("Periodic trigger Probe(probe_id='%s') just issued its last call!", probe->probe_id.c_str());
              need_reschedule = true;
              probes.erase(probe->probe_id);
              probe_it = slice.probes.erase(probe_it);
              delete probe;
              erased = true;
            }
          }
        }
        if (!erased)
          ++probe_it;
      }

      // Reset probes
      for (auto * probe : slice.probes) {
        probe->initialized = true;
        xbt_assert(probe->data_accumulation_strategy == batprotocol::fb::ProbeDataAccumulationStrategy_ProbeDataAccumulation, "non-accumulative probes are not supported right now");
        xbt_assert(probe->data_accumulation_reset_mode == batprotocol::fb::ResetMode_NoReset, "accumulative probes with reset are not implemented");
        // TODO: implement reset
      }

      send_message("server", IPMessageType::PERIODIC_TRIGGER, static_cast<void*>(msg));
    }

    if (need_reschedule) {
      generate_static_periodic_schedule(cml_triggers, probes, static_schedule, slice_duration);
    }
  }
}

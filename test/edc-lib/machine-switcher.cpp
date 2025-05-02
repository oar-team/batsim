#include <batprotocol.hpp>
#include <cstdint>
#include <list>
#include <string>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;


MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint32_t platform_nb_hosts;
IntervalSet available_hosts;

// For the different test options
bool do_turn_on_fail = false;
bool do_turn_off_fail = false;
bool do_job_running_fail = false;
bool do_job_execute_fail = false;

uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size)
{
    if (init_size > 0) {
        std::string init_string((const char *)init_data, static_cast<size_t>(init_size));
        try {
            auto init_json = json::parse(init_string);

            if (init_json.contains("format_json"))
            {
                format_binary = !(init_json["format_json"]);
            }

            if (init_json.contains("option"))
            {
                std::string init_string = init_json["option"];

                if (init_string == "turn_on_fail")
                    do_turn_on_fail = true;

                if (init_string == "turn_off_fail")
                    do_turn_off_fail = true;

                if (init_string == "job_running_fail")
                    do_job_running_fail = true;

                if (init_string == "job_execute_fail")
                    do_job_execute_fail = true;
            }
        } catch (const json::exception & e) {
            throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
        }
    }

    mb = new MessageBuilder(!format_binary);
    *flags = format_binary ? BATSIM_EDC_FORMAT_BINARY : BATSIM_EDC_FORMAT_JSON;

    mb->add_edc_hello("machine-switcher", "0.1.0");
    mb->finish_message(0.0);
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(reply_data), reply_size);

    return 0;
}

uint8_t batsim_edc_deinit()
{
    delete mb;
    mb = nullptr;

    return 0;
}

uint8_t batsim_edc_take_decisions(
    const uint8_t * what_happened,
    uint32_t what_happened_size,
    uint8_t ** decisions,
    uint32_t * decisions_size)
{
    (void) what_happened_size;
    auto * parsed = deserialize_message(*mb, !format_binary, what_happened);
    mb->clear(parsed->now());
    double time_now = parsed->now();

    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();

            platform_nb_hosts = simu_begins->computation_host_number();
            available_hosts = IntervalSet::ClosedInterval(0, platform_nb_hosts-1);

            mb->add_call_me_later("call_at_50", TemporalTrigger::make_one_shot(500));

        } break;
        case fb::Event_JobSubmittedEvent: {
            auto job_event = event->event_as_JobSubmittedEvent();

            if (do_job_execute_fail)
            {
                // Turn off machines before job starts
                mb->add_turn_onoff_hosts(available_hosts.to_string_hyphen(), 1);
                time_now += 1.0;
                mb->set_current_time(time_now);
            }

            auto hosts = IntervalSet(IntervalSet::ClosedInterval(0, job_event->job()->resource_request()-1));
            mb->add_execute_job(job_event->job_id()->str(), hosts.to_string_hyphen());

            if (do_job_running_fail)
            {
                // Turn off machines after job starts
                time_now += 1.0;
                mb->set_current_time(time_now);
                mb->add_turn_onoff_hosts(available_hosts.to_string_hyphen(), 1);
            }

        } break;
        case fb::Event_RequestedCallEvent: {
            auto e = event->event_as_RequestedCallEvent();

            if (e->call_me_later_id()->str() == "call_at_50")
            {
                // Ask to turn ON the hosts (pstate 0)
                mb->add_turn_onoff_hosts(available_hosts.to_string_hyphen(), 0);
            }
        } break;
        case fb::Event_JobCompletedEvent: {
            // Ask to turn OFF the hosts (pstate 1)
            mb->add_turn_onoff_hosts(available_hosts.to_string_hyphen(), 1);
        } break;
        case fb::Event_HostsTurnedOnOffEvent: {
            auto e = event->event_as_HostsTurnedOnOffEvent();

            if ((do_turn_on_fail) and (e->state() == 0))
            {
                // Ask again to turn ON already ON hosts
                mb->add_turn_onoff_hosts(available_hosts.to_string_hyphen(), 0);
            }

            if ((do_turn_off_fail) and (e->state() == 1))
            {
                // Ask again to turn OFF already OFF hosts
                mb->add_turn_onoff_hosts(available_hosts.to_string_hyphen(), 1);
            }
        } break;
        default: break;
        }
    }

    mb->finish_message(time_now);
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

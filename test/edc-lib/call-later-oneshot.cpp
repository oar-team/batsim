#include <batprotocol.hpp>
#include <cstdint>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used
bool issue_all_calls_at_start = false;
batprotocol::fb::TimeUnit time_unit = batprotocol::fb::TimeUnit_Second;
std::vector<uint64_t> calls;
uint32_t next_call = 0;
std::unordered_map<std::string, bool> received_calls;

std::string gen_call_id(uint64_t call_time) {
    return std::string("oneshot_") + std::to_string(call_time);
}

uint8_t batsim_edc_init(const uint8_t * data, uint32_t size, uint32_t flags)
{
    format_binary = ((flags & BATSIM_EDC_FORMAT_BINARY) != 0);
    if ((flags & (BATSIM_EDC_FORMAT_BINARY | BATSIM_EDC_FORMAT_JSON)) != flags)
    {
        printf("Unknown flags used, cannot initialize myself.\n");
        return 1;
    }

    mb = new MessageBuilder(!format_binary);

    std::string init_string((const char *)data, static_cast<size_t>(size));
    try {
        auto init_json = json::parse(init_string);
        issue_all_calls_at_start = init_json["issue_all_calls_at_start"];
        if (init_json["use_ms_time_unit"])
            time_unit = batprotocol::fb::TimeUnit_Millisecond;
        init_json["calls"].get_to(calls);
        received_calls.clear();
        for (uint64_t & call_time : calls) {
            auto call_id = gen_call_id(call_time);
            received_calls[call_id] = false;
        }
    } catch (const json::exception & e) {
        throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
    }

    return 0;
}

uint8_t batsim_edc_deinit()
{
    delete mb;
    mb = nullptr;
    calls.clear();
    received_calls.clear();

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

    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_edc_hello("rejecter", "0.1.0");
        } break;
        case fb::Event_SimulationBeginsEvent: {
            if (issue_all_calls_at_start) {
                for (const uint64_t & call_time : calls) {
                    auto when = TemporalTrigger::make_one_shot(call_time);
                    when->set_time_unit(time_unit);
                    auto call_id = gen_call_id(call_time);
                    mb->add_call_me_later(call_id, when);
                }
            }
            else {
                if (!calls.empty()) {
                    auto call_time = calls.at(next_call);
                    auto when = TemporalTrigger::make_one_shot(call_time);
                    when->set_time_unit(time_unit);
                    auto call_id = gen_call_id(call_time);
                    mb->add_call_me_later(call_id, when);
                    ++next_call;
                }
            }
        } break;
        case fb::Event_SimulationEndsEvent: {
            bool abort = false;
            for (const auto& [id, received]: received_calls) {
                if (!received) {
                    abort = true;
                    printf("SimulationEnds received while call '%s' has not been received yet...\n", id.c_str());
                }
            }
            if (abort)
                throw std::runtime_error("SimulationEnds received while some requested calls were not sent");
        } break;
        case fb::Event_JobSubmittedEvent: {
            auto job_id = event->event_as_JobSubmittedEvent()->job_id()->str();
            mb->add_reject_job(job_id);
        } break;
        case fb::Event_RequestedCallEvent: {
            auto e = event->event_as_RequestedCallEvent();
            auto it = received_calls.find(e->call_me_later_id()->str());
            if (it == received_calls.end())
                throw std::runtime_error("unexpected call_me_later id received");
            if (it->second == true)
                throw std::runtime_error("multiple requested calls received on the same call_id, which should be impossible");
            it->second = true;
            if (e->last_periodic_call())
                throw std::runtime_error("got a true last_periodic_call, which should be impossible");

            if (!issue_all_calls_at_start && next_call < calls.size()) {
                auto call_time = calls.at(next_call);
                auto when = TemporalTrigger::make_one_shot(call_time);
                when->set_time_unit(time_unit);
                auto call_id = gen_call_id(call_time);
                mb->add_call_me_later(call_id, when);
                ++next_call;
            }
        } break;
        default: break;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

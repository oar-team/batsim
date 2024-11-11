#include <cstdint>

#include <batprotocol.hpp>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

struct PeriodicCall {
    std::string call_id; // periodic call_id
    std::string oneshot_call_id;
    bool is_probe = false; // if true, issue a periodic probe instead of a periodic call me later
    bool probe_just_created = false; // set to true for probes when they were created but no data has been received from them yet
    uint64_t init_time = 0;
    uint64_t period;
    double expected_period_s = -1;
    uint64_t expected_nb_calls;
    uint64_t nb_calls = 0;
    double previous_call_time = -1;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used
batprotocol::fb::TimeUnit time_unit = batprotocol::fb::TimeUnit_Second;
double time_unit_multiplier = 1.0;
bool is_infinite;

std::map<std::string, std::string> oneshot_to_periodic_ids; // used to start periodic calls at non-zero times
// all of these call_id are periodic call_id
std::map<std::string, PeriodicCall*> calls;
std::map<std::string, std::set<std::string>> divisers; // contains the all the call_ids whose period is a divisers of each call_id
std::set<std::string> alive_calls;
double float_comp_precision = 1e-3;

std::string gen_call_id(bool is_probe, uint64_t period, uint64_t nb_periods) {
    std::string prefix = "probe_";
    if (!is_probe)
        prefix = "cml_";
    return prefix + std::to_string(period) + "_" + std::to_string(nb_periods);
}

std::string gen_periodic_call_id(bool is_probe, uint64_t period, uint64_t nb_periods) {
    return std::string("period_") + gen_call_id(is_probe, period, nb_periods);
}

std::string gen_oneshot_call_id(bool is_probe, uint64_t period, uint64_t nb_periods) {
    return std::string("oneshot_") + gen_call_id(is_probe, period, nb_periods);
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
        is_infinite = init_json["is_infinite"];
        std::string time_unit_str = init_json["time_unit"];
        if (time_unit_str == "ms") {
            time_unit = batprotocol::fb::TimeUnit_Millisecond;
            time_unit_multiplier = 1e-3;
        }
        else if (time_unit_str == "s") {
            time_unit = batprotocol::fb::TimeUnit_Second;
            time_unit_multiplier = 1.0;
        }
        else
            throw std::runtime_error("unknown time_unit received: " + time_unit_str);
        std::map<uint64_t, std::set<PeriodicCall*>> periods;
        for (auto call : init_json["calls"]) {
            auto c = new PeriodicCall{
                "", "",
                call["is_probe"], false,
                call["init"],
                call["period"],
                (double)(call["period"]) * time_unit_multiplier,
                call["nb"],
                0, -1
            };
            auto call_id = gen_periodic_call_id(call["is_probe"], call["period"], call["nb"]);
            c->call_id = call_id;
            c->oneshot_call_id = gen_oneshot_call_id(call["is_probe"], call["period"], call["nb"]);
            calls[call_id] = c;
            oneshot_to_periodic_ids[c->oneshot_call_id] = c->call_id;

            auto it = periods.find(c->period);
            if (it == periods.end())
                periods[c->period] = {c};
            else
                it->second.insert(c);
        }

        // check that all periods are multiple of each other
        for (auto& [call_id1, call1]: calls) {
            for (auto& [call_id2, call2]: calls) {
                if (call_id1 != call_id2) {
                    auto * ca = call1;
                    auto * cb = call2;
                    if (ca->period > cb->period) {
                        auto * tmp = ca;
                        ca = cb;
                        cb = tmp;
                    }
                    if (cb->period % ca->period != 0) {
                        fprintf(stderr, "  invalid input: periods %lu and %lu are not multiple\n", ca->period, cb->period);
                        throw std::runtime_error("invalid periods");
                    } else {
                        divisers[cb->call_id].insert(ca->call_id);
                    }
                }
            }
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
    for (auto & [_, call] : calls)
        delete call;
    calls.clear();

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

    bool received_call = false;
    std::set<std::string> triggered_calls;
    std::set<std::string> calls_to_make_alive;
    std::set<std::string> calls_to_remove;

    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_edc_hello("call-later-periodic", "0.1.0");
        } break;
        case fb::Event_SimulationBeginsEvent: {
            for (const auto & [call_id, call] : calls) {
                if (call->init_time == 0) {
                    fprintf(stderr, "  initiating call_id=%s\n", call_id.c_str());
                    std::shared_ptr<TemporalTrigger> when;
                    if (is_infinite)
                        when = TemporalTrigger::make_periodic(call->period);
                    else
                        when = TemporalTrigger::make_periodic_finite(call->period, call->expected_nb_calls);
                    when->set_time_unit(time_unit);
                    if (call->is_probe) {
                        auto cp = batprotocol::CreateProbe::make_temporal_triggerred(when);
                        cp->set_resources_as_hosts("0");
                        cp->enable_accumulation_no_reset();
                        mb->add_create_probe(call_id, batprotocol::fb::Metrics_Power, cp);
                        call->probe_just_created = true;
                    } else {
                        mb->add_call_me_later(call_id, when);
                        alive_calls.insert(call_id);
                    }
                } else {
                    // run a one shot call, that will initiate the periodic call when triggered
                    fprintf(stderr, "  initiating oneshot call_id=%s\n", call->oneshot_call_id.c_str());
                    auto when = TemporalTrigger::make_one_shot(call->init_time);
                    when->set_time_unit(time_unit);
                    mb->add_call_me_later(call->oneshot_call_id, when);
                }
            }
        } break;
        case fb::Event_SimulationEndsEvent: {
            bool abort = false;
            for (const auto& [call_id, call]: calls) {
                if (call->nb_calls != call->expected_nb_calls) {
                    abort = true;
                    printf("SimulationEnds received while only %lu/%lu calls have been received from '%s' yet...\n", call->nb_calls, call->expected_nb_calls, call_id.c_str());
                    fflush(stdout);
                }
            }
            if (abort)
                throw std::runtime_error("SimulationEnds received while some requested calls were not sent");
            else {
                printf("Received all calls as expected\n");
                fflush(stdout);
            }
        } break;
        case fb::Event_JobSubmittedEvent: {
            auto job_id = event->event_as_JobSubmittedEvent()->job_id()->str();
            mb->add_reject_job(job_id);
        } break;
        case fb::Event_ProbeDataEmittedEvent:
        case fb::Event_RequestedCallEvent: {
            if (!received_call)
                fprintf(stderr, "  time=%g, received calls!\n", event->timestamp());
            received_call = true;

            auto e = event->event_as_RequestedCallEvent();
            auto ep = event->event_as_ProbeDataEmittedEvent();
            std::string call_id = "unset";
            if (e != nullptr) { // requested call, not a probe
                call_id = e->call_me_later_id()->str();
                auto oneshot_it = oneshot_to_periodic_ids.find(call_id);
                if (oneshot_it != oneshot_to_periodic_ids.end()) {
                    fprintf(stderr, "    got oneshot trigger %s\n", call_id.c_str());
                    auto & call_id = oneshot_it->second;
                    auto & call = calls.at(call_id);
                    fprintf(stderr, "    initiating call_id=%s\n", call_id.c_str());
                    std::shared_ptr<TemporalTrigger> when;
                    if (is_infinite)
                        when = TemporalTrigger::make_periodic(call->period);
                    else
                        when = TemporalTrigger::make_periodic_finite(call->period, call->expected_nb_calls);
                    when->set_time_unit(time_unit);
                    if (call->is_probe) {
                        auto cp = batprotocol::CreateProbe::make_temporal_triggerred(when);
                        cp->set_resources_as_hosts("0");
                        cp->enable_accumulation_no_reset();
                        mb->add_create_probe(call_id, batprotocol::fb::Metrics_Power, cp);
                        call->probe_just_created = true;
                    } else {
                        mb->add_call_me_later(call_id, when);

                        if (alive_calls.find(call_id) != alive_calls.end())
                            throw std::runtime_error("unexpected state: waiting to set a call alive while it is already alive, call_id=" + call_id);
                        calls_to_make_alive.insert(call_id);
                    }

                    oneshot_to_periodic_ids.erase(oneshot_it);
                    break;
                }
            } else {
                call_id = ep->probe_id()->str();
            }

            auto it = calls.find(call_id);
            if (it == calls.end())
                throw std::runtime_error("unexpected call_me_later id received");

            auto & call = it->second;
            ++call->nb_calls;
            fprintf(stderr, "    %lu/%lu of call='%s'\n", call->nb_calls, call->expected_nb_calls, it->first.c_str());

            if (call->probe_just_created) {
                call->probe_just_created = false;

                if (alive_calls.find(call->call_id) != alive_calls.end())
                    throw std::runtime_error("unexpected state: waiting to set a call alive while it is already alive, call_id=" + call->call_id);
                alive_calls.insert(call->call_id);
            }

            bool all_calls_received = call->nb_calls == call->expected_nb_calls;
            if (is_infinite && all_calls_received) {
                if (call->is_probe)
                    mb->add_stop_probe(call->call_id);
                else
                    mb->add_stop_call_me_later(call->call_id);
            }

            if (!is_infinite && !call->is_probe && e->last_periodic_call() != all_calls_received) {
                char * err_cstr;
                asprintf(&err_cstr, "last_periodic_call inconsistency on '%s': value received is %d while expecting %d, since I received %lu/%lu calls",
                    it->first.c_str(),
                    (int)e->last_periodic_call(), (int)all_calls_received,
                    call->nb_calls, call->expected_nb_calls
                );
                std::string err(err_cstr);
                free(err_cstr);
                throw std::runtime_error(err);
            }

            if (call->previous_call_time != -1) {
                double elapsed_time = event->timestamp() - call->previous_call_time;
                if (fabs(elapsed_time - (double)call->expected_period_s) > float_comp_precision) {
                    fprintf(stderr, "    time elapsed since last call is %g, which is farther away from expected period of %g s than the accepted threshold %g, aborting\n", elapsed_time, call->expected_period_s, float_comp_precision);
                    throw std::runtime_error("periodic call period inconsistency");
                }
            }
            call->previous_call_time = event->timestamp();

            // state checks
            if (all_calls_received)
                calls_to_remove.insert(call->call_id);
            auto triggered_it = triggered_calls.find(call->call_id);
            if (triggered_it != triggered_calls.end())
                throw std::runtime_error("got several triggers from the same periodic entity in this message, call_id=" + call->call_id);
            triggered_calls.insert(call->call_id);

            auto alive_it = alive_calls.find(call->call_id);
            if (alive_it == alive_calls.end())
                throw std::runtime_error("got a trigger from a periodic entity that should no longer be active, call_id=" + call->call_id);
        } break;
        default: break;
        }
    }

    // check that the alive divisers of all received calls also got triggered in the same Batsim message
    bool abort = false;
    for (const auto & triggered_call_id : triggered_calls) {
        for (const auto & diviser_call_id : divisers[triggered_call_id]) {
            if (alive_calls.find(diviser_call_id) != alive_calls.end()) {
                if (triggered_calls.find(diviser_call_id) == triggered_calls.end()) {
                    fprintf(stderr, "  call '%s' triggered, but I was expecting call '%s' to also be triggered in the same message, which did not happen\n", triggered_call_id.c_str(), diviser_call_id.c_str());
                    abort = true;
                } else {
                    fprintf(stderr, "    group trigger ok, '%s'->'%s' as expected\n", triggered_call_id.c_str(), diviser_call_id.c_str());
                }
            }
        }
    }
    if (abort)
        throw std::runtime_error("periodic entity that should have been triggered in the same message were not, aborting");

    // clear calls that have finished
    for (auto & call_id : calls_to_remove)
        alive_calls.erase(call_id);

    // make alive calls that were initiated this round
    for (auto & call_id : calls_to_make_alive)
        alive_calls.insert(call_id);

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

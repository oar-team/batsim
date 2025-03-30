#include <batprotocol.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <list>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

double EPSILON = 1e-3;

struct ExternalEvent {
    double date;
    std::string data;
};
std::list<::ExternalEvent> expected_external_events;

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

        std::vector<std::string> external_event_filenames;
        external_event_filenames = init_json["external_event_filenames"];

        for (const std::string & event_filename : external_event_filenames) {
            std::ifstream f(event_filename);
            std::string line;
            while (std::getline(f, line)) {
                auto line_json = json::parse(line);
                ::ExternalEvent e;
                e.date = line_json["timestamp"];
                e.data = line_json["data"];
                expected_external_events.push_back(e);
            }
        }

        expected_external_events.sort([](const auto & a, const auto & b) {
            return a.date < b.date;
        });

        printf("there are %zu expected events\n", expected_external_events.size());
    } catch (const json::exception & e) {
        throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
    }

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

    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_edc_hello("external-event-check", "0.1.0");
        } break;
        case fb::Event_ExternalEventOccurredEvent: {
            if (expected_external_events.empty()) {
                throw std::runtime_error("received an unexpected external event");
            }
            auto & expected_event = expected_external_events.front();
            printf("time=%g, expecting next external event at time=%g with data='%s'\n", parsed->now(), expected_event.date, expected_event.data.c_str());
            fflush(stdout);
            if (parsed->now() > expected_event.date + EPSILON) {
                throw std::runtime_error("received an external event at an unexpected time");
            }
            auto ee_event_data = event->event_as_ExternalEventOccurredEvent()->external_event_as_GenericExternalEvent()->data()->str();
            if (expected_event.data != ee_event_data) {
                throw std::runtime_error("received an external event with unexpected data");
            }
            printf("time=%g, received external event data='%s'\n\n", parsed->now(), ee_event_data.c_str());
            expected_external_events.pop_front();
        } break;
        case fb::Event_JobSubmittedEvent: {
            auto job_id = event->event_as_JobSubmittedEvent()->job_id()->str();
            mb->add_reject_job(job_id);
        } break;
        default: break;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

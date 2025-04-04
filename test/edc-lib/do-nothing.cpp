#include <batprotocol.hpp>
#include <cstdint>

#include <batprotocol.hpp>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used
bool handle_hello = true; // whether this EDC should reply to Batsim's hello event (it should to be valid)

uint8_t batsim_edc_init(const uint8_t * data, uint32_t size, uint32_t flags)
{
    format_binary = ((flags & BATSIM_EDC_FORMAT_BINARY) != 0);
    if ((flags & (BATSIM_EDC_FORMAT_BINARY | BATSIM_EDC_FORMAT_JSON)) != flags)
    {
        printf("Unknown flags used, cannot initialize myself.\n");
        return 1;
    }

    mb = new MessageBuilder(!format_binary);

    if (size > 0) {
        std::string init_string((const char *)data, static_cast<size_t>(size));
        try {
            auto init_json = json::parse(init_string);
            handle_hello = init_json["handle_hello"];
        } catch (const json::exception & e) {
            throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
        }
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

    if (handle_hello) {
        auto nb_events = parsed->events()->size();
        for (unsigned int i = 0; i < nb_events; ++i)
        {
            auto event = (*parsed->events())[i];
            switch (event->event_type())
            {
            case fb::Event_BatsimHelloEvent: {
                mb->add_edc_hello("do-nothing", "0.1.0");
            } break;
            default: break;
            }
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

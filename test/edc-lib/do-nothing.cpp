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

uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size)
{
    if (init_size > 0) {
        std::string init_string((const char *)init_data, static_cast<size_t>(init_size));
        try {
            auto init_json = json::parse(init_string);

            if (init_json.contains("handle_hello"))
            {
                handle_hello = init_json["handle_hello"];
            }

            if (init_json.contains("format_json"))
            {
                format_binary = !(init_json["format_json"]);
            }
        } catch (const json::exception & e) {
            throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
        }
    }

    mb = new MessageBuilder(!format_binary);
    *flags = format_binary ? BATSIM_EDC_FORMAT_BINARY : BATSIM_EDC_FORMAT_JSON;

    if (handle_hello)
    {
        mb->add_edc_hello("do-nothing", "0.1.0");
    }
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

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

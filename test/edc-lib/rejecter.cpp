#include <batprotocol.hpp>
#include <cstdint>

#include "batsim_edc.h"

using namespace batprotocol;

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size)
{
    // ignore initialization data
    (void) init_data;
    (void) init_size;

    mb = new MessageBuilder(!format_binary);
    *flags = format_binary ? BATSIM_EDC_FORMAT_BINARY : BATSIM_EDC_FORMAT_JSON;

    mb->add_edc_hello("rejecter", "0.1.0");
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

    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
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

#include <batprotocol.hpp>
#include <cstdint>

#include "batsim_edc.h"

using namespace batprotocol;

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint8_t batsim_edc_init(const uint8_t * data, uint8_t size, uint8_t flags)
{
    format_binary = ((flags & BATSIM_EDC_FORMAT_BINARY) != 0);
    if ((flags & (BATSIM_EDC_FORMAT_BINARY | BATSIM_EDC_FORMAT_JSON)) != flags)
    {
        printf("Unknown flags used, cannot initialize myself.\n");
        return 1;
    }

    mb = new MessageBuilder(!format_binary);

    // rejecter ignores initialization data
    (void) data;
    (void) size;

    return 0;
}

uint8_t batsim_edc_deinit()
{
    delete mb;
    mb = nullptr;

    return 0;
}

uint8_t batsim_edc_take_decisions(const uint8_t * what_happened, uint8_t ** decisions)
{
    uint8_t * input_buffer = const_cast<uint8_t *>(what_happened);
    if (!format_binary)
        mb->parse_json_message((const char *)what_happened, input_buffer);
    auto parsed = flatbuffers::GetRoot<fb::Message>(input_buffer);
    mb->clear(parsed->now());

    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_external_decision_component_hello("rejecter", "0.1.0");
        } break;
        case fb::Event_JobSubmittedEvent: {
            auto job_id = event->event_as_JobSubmittedEvent()->job_id()->str();
            mb->add_reject_job(job_id);
        } break;
        }
    }

    mb->finish_message(parsed->now());
    if (format_binary)
        *decisions = (uint8_t*) mb->buffer_pointer();
    else
        *decisions = (uint8_t*) mb->buffer_as_json()->c_str();
    return 0;
}

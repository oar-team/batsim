#include <cstdint>
#include <list>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include "batsim_edc.h"

using namespace batprotocol;

struct SchedJob
{
    std::string job_id;
    uint8_t nb_hosts;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used
std::list<SchedJob*> * jobs = nullptr;
SchedJob * currently_running_job = nullptr;
uint32_t platform_nb_hosts = 0;

uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size)
{
    // ignore initialization data
    (void) init_data;
    (void) init_size;

    mb = new MessageBuilder(!format_binary);
    *flags = format_binary ? BATSIM_EDC_FORMAT_BINARY : BATSIM_EDC_FORMAT_JSON;
    jobs = new std::list<SchedJob*>();

    mb->add_edc_hello("exec1by1", "0.1.0");
    mb->finish_message(0.0);
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(reply_data), reply_size);

    return 0;
}

uint8_t batsim_edc_deinit()
{
    delete mb;
    mb = nullptr;

    if (jobs != nullptr)
    {
        for (auto * job : *jobs)
        {
            delete job;
        }
        delete jobs;
        jobs = nullptr;
    }

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
        printf("exec1by1 received event type='%s'\n", batprotocol::fb::EnumNamesEvent()[event->event_type()]);
        switch (event->event_type())
        {
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();
            platform_nb_hosts = simu_begins->computation_host_number();
        } break;
        case fb::Event_JobSubmittedEvent: {
            auto parsed_job = event->event_as_JobSubmittedEvent();
            auto job = new SchedJob();
            job->job_id = parsed_job->job_id()->str();

            job->nb_hosts = parsed_job->job()->resource_request();
            if (job->nb_hosts > platform_nb_hosts)
            {
                mb->add_reject_job(job->job_id);
                delete job;
            }
            else
            {
                jobs->push_back(job);
            }
        } break;
        case fb::Event_JobCompletedEvent: {
            delete currently_running_job;
            currently_running_job = nullptr;
        } break;
        default: break;
        }
    }

    if (currently_running_job == nullptr && !jobs->empty())
    {
        currently_running_job = jobs->front();
        jobs->pop_front();
        auto hosts = IntervalSet(IntervalSet::ClosedInterval(0, currently_running_job->nb_hosts-1));
        mb->add_execute_job(currently_running_job->job_id, hosts.to_string_hyphen());
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

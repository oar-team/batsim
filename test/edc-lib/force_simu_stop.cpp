#include <batprotocol.hpp>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include "batsim_edc.h"

using namespace batprotocol;

struct Job
{
    std::string id;
    uint32_t nb_hosts;
    IntervalSet alloc;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint32_t platform_nb_hosts = 0;
uint32_t nb_available_hosts = 0;
IntervalSet available_hosts;
std::list<::Job*> job_queue;
std::unordered_map<std::string, ::Job*> running_jobs;
::Job * last_running_job = nullptr;

uint8_t batsim_edc_init(const uint8_t * data, uint32_t size, uint32_t flags)
{
    format_binary = ((flags & BATSIM_EDC_FORMAT_BINARY) != 0);
    if ((flags & (BATSIM_EDC_FORMAT_BINARY | BATSIM_EDC_FORMAT_JSON)) != flags)
    {
        printf("Unknown flags used, cannot initialize myself.\n");
        return 1;
    }

    mb = new MessageBuilder(!format_binary);

    // ignore initialization data
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

uint8_t batsim_edc_take_decisions(
    const uint8_t * what_happened,
    uint32_t what_happened_size,
    uint8_t ** decisions,
    uint32_t * decisions_size)
{
    (void) what_happened_size;
    auto * parsed = deserialize_message(*mb, !format_binary, what_happened);
    mb->clear(parsed->now());

    bool need_scheduling = false;
    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_edc_hello("simulation_stopper", "0.1.0");
        } break;
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();
            platform_nb_hosts = simu_begins->computation_host_number();
            nb_available_hosts = platform_nb_hosts;
            available_hosts = IntervalSet::ClosedInterval(0, platform_nb_hosts - 1);

            auto when = TemporalTrigger::make_one_shot(10);
            when->set_time_unit(batprotocol::fb::TimeUnit_Second);
            mb->add_call_me_later("call_at_10", when);
        } break;
        case fb::Event_JobSubmittedEvent: {
            ::Job job{
                event->event_as_JobSubmittedEvent()->job_id()->str(),
                event->event_as_JobSubmittedEvent()->job()->resource_request(),
                IntervalSet::empty_interval_set()
            };

            if (job.nb_hosts > platform_nb_hosts)
                mb->add_reject_job(job.id);
            else {
                need_scheduling = true;
                job_queue.emplace_back(new ::Job(job));
            }
        } break;
        case fb::Event_JobCompletedEvent: {
            need_scheduling = true;

            auto job_id = event->event_as_JobCompletedEvent()->job_id()->str();
            auto job_it = running_jobs.find(job_id);
            auto job = job_it->second;
            nb_available_hosts += job->nb_hosts;
            available_hosts += job->alloc;

            delete job;
            running_jobs.erase(job_it);
        } break;
        case fb::Event_RequestedCallEvent: {
            if (last_running_job != nullptr)
            {
                mb->add_kill_jobs({last_running_job->id});
            }

            mb->set_current_time(parsed->now() + 1);
            mb->add_force_simulation_stop();
        } break;
        default: break;
        }
    }

    if (need_scheduling) {
        for (auto job_it = job_queue.begin(); job_it != job_queue.end(); ) {
            auto job = *job_it;
            if (job->nb_hosts <= nb_available_hosts) {
                running_jobs[job->id] = *job_it;
                job->alloc = available_hosts.left(job->nb_hosts);
                mb->add_execute_job(job->id, job->alloc.to_string_hyphen());
                available_hosts -= job->alloc;
                nb_available_hosts -= job->nb_hosts;

                job_it = job_queue.erase(job_it);

                last_running_job = job;
            }
            else
                break;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

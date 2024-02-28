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
    double walltime;

    IntervalSet alloc;
    double maximum_finish_time;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint32_t platform_nb_hosts = 0;
std::list<::Job*> job_queue;
std::unordered_map<std::string, ::Job*> running_jobs;
uint32_t nb_available_hosts = 0;
IntervalSet available_hosts;

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

bool ascending_max_finish_time_job_order(const ::Job* a, const ::Job* b) {
    return a->maximum_finish_time < b->maximum_finish_time;
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
    for (unsigned int i = 0; i < nb_events; ++i) {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_edc_hello("easy", "0.1.0");
        } break;
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();
            platform_nb_hosts = simu_begins->computation_host_number();
            nb_available_hosts = platform_nb_hosts;
            available_hosts = IntervalSet::ClosedInterval(0, platform_nb_hosts - 1);
        } break;
        case fb::Event_JobSubmittedEvent: {
            ::Job job{
                event->event_as_JobSubmittedEvent()->job_id()->str(),
                event->event_as_JobSubmittedEvent()->job()->resource_request(),
                event->event_as_JobSubmittedEvent()->job()->walltime(),
                IntervalSet::empty_interval_set(),
                -1
            };

            if (job.nb_hosts > platform_nb_hosts)
                mb->add_reject_job(job.id);
            else if (job.walltime <= 0)
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
        default: break;
        }
    }

    if (need_scheduling) {
        ::Job* priority_job = nullptr;
        uint32_t nb_available_hosts_at_priority_job_start = 0;
        float priority_job_start_time = -1;

        // First traversal, done until a job cannot be executed right now and is set as the priority job
        // (or until all jobs have been executed)
        auto job_it = job_queue.begin();
        for (; job_it != job_queue.end(); ) {
            auto job = *job_it;
            if (job->nb_hosts <= nb_available_hosts) {
                running_jobs[job->id] = *job_it;
                job->maximum_finish_time = parsed->now() + job->walltime;
                job->alloc = available_hosts.left(job->nb_hosts);
                mb->add_execute_job(job->id, job->alloc.to_string_hyphen());
                available_hosts -= job->alloc;
                nb_available_hosts -= job->nb_hosts;

                job_it = job_queue.erase(job_it);
            }
            else
            {
                priority_job = *job_it;
                ++job_it;

                // compute when the priority job can start, and the number of available machines at this time
                std::vector<::Job*> running_jobs_asc_maximum_finish_time;
                running_jobs_asc_maximum_finish_time.reserve(running_jobs.size());
                for (const auto & it : running_jobs)
                    running_jobs_asc_maximum_finish_time.push_back(it.second);
                std::sort(running_jobs_asc_maximum_finish_time.begin(), running_jobs_asc_maximum_finish_time.end(), ascending_max_finish_time_job_order);

                nb_available_hosts_at_priority_job_start = nb_available_hosts;
                for (const auto & job : running_jobs_asc_maximum_finish_time) {
                    nb_available_hosts_at_priority_job_start += job->nb_hosts;
                    if (nb_available_hosts_at_priority_job_start >= priority_job->nb_hosts) {
                        nb_available_hosts_at_priority_job_start -= priority_job->nb_hosts;
                        priority_job_start_time = job->maximum_finish_time;
                        break;
                    }
                }

                break;
            }
        }

        // Continue traversal to backfill jobs
        for (; job_it != job_queue.end(); ) {
            auto job = *job_it;
            // should the job be backfilled?
            float job_finish_time = parsed->now() + job->walltime;
            if (job->nb_hosts <= nb_available_hosts && // enough resources now?
                (job->nb_hosts <= nb_available_hosts_at_priority_job_start || job_finish_time <= priority_job_start_time)) {  // does not directly hinder the priority job?
                running_jobs[job->id] = *job_it;
                job->maximum_finish_time = job_finish_time;
                job->alloc = available_hosts.left(job->nb_hosts);
                mb->add_execute_job(job->id, job->alloc.to_string_hyphen());
                available_hosts -= job->alloc;
                nb_available_hosts -= job->nb_hosts;

                if (job_finish_time > priority_job_start_time)
                nb_available_hosts_at_priority_job_start -= job->nb_hosts;

                job_it = job_queue.erase(job_it);
            }
            else if (nb_available_hosts == 0)
                break;
            else
                ++job_it;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

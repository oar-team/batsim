#include <batprotocol.hpp>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

struct InternalJob
{
    std::string id;
    uint32_t nb_hosts;
    IntervalSet alloc;
    std::string profile_name;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint32_t platform_nb_hosts = 0;
std::list<::InternalJob*> job_queue;
std::unordered_map<std::string, ::InternalJob*> running_jobs;
uint32_t nb_available_hosts = 0;
IntervalSet available_hosts;
InternalJob* first_job = nullptr;
bool need_scheduling = false;

// For the different test options
bool do_dynamic_submissions = false;
std::string init_string;
bool ack_job_registration = true;
bool do_once = true;

uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size)
{
    // Retrieve the dynamic registrations to perform
    try {
        auto init_json = json::parse(std::string((const char *)init_data, static_cast<size_t>(init_size)));
        init_string = init_json["option"];
    } catch (const json::exception & e) {
        throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
    }

    if (init_string == "noack_jobs_ok")
    {
        ack_job_registration = false;
    }

    mb = new MessageBuilder(!format_binary);
    *flags = format_binary ? BATSIM_EDC_FORMAT_BINARY : BATSIM_EDC_FORMAT_JSON;

    // Set simulation feature request
    EDCHelloOptions options = EDCHelloOptions();
    options.request_dynamic_registration();

    if (ack_job_registration)
    {
        options.request_acknowledge_dynamic_jobs();
    }

    if (init_string == "profile_reuse_ok")
    {
        options.request_profile_reuse();
    }

    mb->add_edc_hello("dynamic_register", "0.1.0", "nocommit", options);
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

void add_new_job(std::string job_id, std::string profile_id, uint32_t resource_request)
{
    InternalJob job{job_id,
        resource_request,
        IntervalSet::empty_interval_set(),
        profile_id};

    if (first_job == nullptr)
    {
        // Keep first job in memory
        first_job = new InternalJob(job);
        do_dynamic_submissions = true;
    }
    else
    {
        need_scheduling = true;
        job_queue.emplace_back(new InternalJob(job));
    }
}

void do_registration_ok(std::string first_profile_id)
{
    // Re-use profile of static workload, register job in new workload
    auto j1 = batprotocol::Job::make();
    j1->set_resource_number(2);
    j1->set_walltime(5);
    j1->set_profile(first_profile_id);
    mb->add_register_job("dyn!j1", j1);

    if (!ack_job_registration)
    {
        add_new_job("dyn!j1", first_profile_id, 2);
    }

    // Register profile in new workload, register job in static workload
    auto p2 = batprotocol::Profile::make_parallel_task_homogeneous(
        batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsUsedForEachValue,
        1e9,
        5e4);
    std::string prof2 = "dyn!profile2";
    mb->add_register_profile(prof2, p2);

    auto j2 = batprotocol::Job::make();
    j2->set_resource_number(4);
    j2->set_profile(prof2);
    mb->add_register_job("w0!j2", j2);

    if (!ack_job_registration)
    {
        add_new_job("w0!j2", prof2, 4);
    }

    // Register same profile name in new workload (should work)
    auto p3 = batprotocol::Profile::make_delay(5);
    mb->add_register_profile("dyn!delay10", p3);

    mb->add_reject_job(first_job->id);

    // Register various profiles
    auto cpu_vector = std::make_shared<std::vector<double>>(std::vector<double>(4, 100));
    auto com_vector = std::make_shared<std::vector<double>>(std::vector<double>(16, 50));
    auto p4 = batprotocol::Profile::make_parallel_task(cpu_vector, com_vector);
    std::string prof4 = "dyn!simple_ptask";
    mb->add_register_profile(prof4, p4);

    auto p5 = batprotocol::Profile::make_parallel_task(nullptr, com_vector);
    std::string prof5 = "dyn!simple_ptask_nocpu";
    mb->add_register_profile(prof5, p5);

    auto p6 = batprotocol::Profile::make_parallel_task(cpu_vector, nullptr);
    std::string prof6 = "dyn!simple_ptask_nocom";
    mb->add_register_profile(prof6, p6);

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

    need_scheduling = false;
    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();
            platform_nb_hosts = simu_begins->computation_host_number();
            nb_available_hosts = platform_nb_hosts;
            available_hosts = IntervalSet::ClosedInterval(0, platform_nb_hosts - 1);
        } break;
        case fb::Event_JobSubmittedEvent: {
            if (event->event_as_JobSubmittedEvent()->job()->resource_request() > platform_nb_hosts)
                mb->add_reject_job(event->event_as_JobSubmittedEvent()->job_id()->str());
            else {
                add_new_job(event->event_as_JobSubmittedEvent()->job_id()->str(),
                            event->event_as_JobSubmittedEvent()->job()->profile_id()->str(),
                            event->event_as_JobSubmittedEvent()->job()->resource_request());
            }


            if ((init_string == "profile_reuse_fail") or (init_string == "profile_reuse_ok"))
            {
                if (do_once)
                {
                    do_once = false;
                    mb->add_reject_job(event->event_as_JobSubmittedEvent()->job_id()->str());

                    auto when = TemporalTrigger::make_one_shot(15);
                    when->set_time_unit(batprotocol::fb::TimeUnit_Second);
                    mb->add_call_me_later("call_at_15", when);

                    do_dynamic_submissions = false;
                }
            }
        } break;
        case fb::Event_JobCompletedEvent: {
            need_scheduling = true;

            auto job_id = event->event_as_JobCompletedEvent()->job_id()->str();
            auto job_it = running_jobs.find(job_id);
            auto job = job_it->second;
            nb_available_hosts += job->nb_hosts;
            available_hosts += job->alloc;

            running_jobs.erase(job_it);
            delete job;
        } break;
        case fb::Event_RequestedCallEvent: {
            do_dynamic_submissions = true;
        } break;
        default: break;
        }
    }

    if (do_dynamic_submissions)
    {
        do_dynamic_submissions = false;

        std::string & first_profile_id = first_job->profile_name;

        if (init_string == "identical_job_names_fail")
        {
            auto j2 = batprotocol::Job::make();
            j2->set_resource_number(4);
            j2->set_profile("w0!delay10");
            mb->add_register_job("w0!1", j2);
        }
        else if (init_string == "identical_profile_names_fail")
        {
            // Register same profile name in static workload (should NOT work)
            auto prof = batprotocol::Profile::make_delay(5);
            mb->add_register_profile(first_profile_id, prof);

        }
        else if ((init_string == "profile_reuse_fail") or (init_string == "profile_reuse_ok"))
        {
            // Try to re-use profile of first job from static workload
            auto j1 = batprotocol::Job::make();
            j1->set_resource_number(2);
            j1->set_walltime(5);
            j1->set_profile(first_profile_id);
            mb->add_register_job("dyn!j1", j1);
        }
        else
        {
            do_registration_ok(first_profile_id);
        }

        mb->add_finish_registration();
    }

    if (need_scheduling) {
        for (auto job_it = job_queue.begin(); job_it != job_queue.end(); ) {
            auto job = *job_it;
            if (job->nb_hosts <= nb_available_hosts) {
                running_jobs[job->id] = *job_it;
                job->alloc = available_hosts.left(job->nb_hosts);
                mb->add_execute_job(job->id, job->alloc.to_string_hyphen(" ", "-"));
                available_hosts -= job->alloc;
                nb_available_hosts -= job->nb_hosts;

                job_it = job_queue.erase(job_it);
            }
            else
                break;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

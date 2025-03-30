#include <cstdint>
#include <limits>
#include <random>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

struct SchedJob {
    uint32_t nb_hosts;
    std::string profile_id;
    std::string job_id;
    std::vector<uint32_t> dvfs_states;
    std::vector<double> comp;
    IntervalSet alloc;
    double expected_runtime;
    double start_time;
};

struct Host {
    std::vector<double> speeds;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used
uint32_t platform_nb_hosts = 0;
std::mt19937 rng;
uint32_t nb_jobs_to_submit = 0;
uint32_t nb_submitted_jobs = 0;
uint32_t nb_pstate_switches_done = 0;
SchedJob job;
std::vector<Host> hosts;

const double EPSILON = 1e-2;

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

        unsigned int seed_value = init_json["random_seed"];
        rng.seed(seed_value);
        nb_jobs_to_submit = init_json["nb_jobs_to_submit"];

        for (uint32_t i = 0; i < nb_jobs_to_submit; ++i) {
            printf("%u: %lu\n", i, rng());
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
    bool register_job_profile = false;
    bool execute_job = false;
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        printf("exec1by1 received event type='%s'\n", batprotocol::fb::EnumNamesEvent()[event->event_type()]);
        switch (event->event_type())
        {
        case fb::Event_BatsimHelloEvent: {
            mb->add_edc_hello("exec1by1-dvfs-dyn", "0.1.0");
        } break;
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();
            platform_nb_hosts = simu_begins->computation_host_number();
            if (platform_nb_hosts < 2) {
                throw std::runtime_error("this edc only supports platforms with 2 hosts or more");
            }
            auto proto_hosts = simu_begins->computation_hosts();
            hosts.clear();
            for (unsigned int i = 0; i < proto_hosts->size(); ++i) {
                auto host = proto_hosts->Get(i);
                BAT_ENFORCE(host->id() == i, "the %u-th computation host has id=%u instead of expected %u", i, host->id(), i);
                BAT_ENFORCE(host->pstate() == 0, "expected pstate=0 for host=%u but got %u", i, host->pstate());
                BAT_ENFORCE(host->pstate_count() == 2, "expected pstate_count=2 for host=%u but got %u", i, host->pstate_count());
                BAT_ENFORCE(host->state() == fb::HostState_IDLE, "host=%u is not in idle state", i);
                BAT_ENFORCE(host->core_count() == 1, "host=%u has %u cores while 1 was expected", i, host->core_count());
                BAT_ENFORCE(host->computation_speed()->size() == 2, "host=%u has %u computation_speeds while 2 were expected", i, host->computation_speed()->size());

                Host h;
                for (unsigned p = 0; p < host->computation_speed()->size(); ++p) {
                    double pstate_speed = host->computation_speed()->Get(p);
                    h.speeds.push_back(pstate_speed);
                }
                hosts.push_back(h);
            }

            register_job_profile = nb_submitted_jobs < nb_jobs_to_submit;
        } break;
        case fb::Event_JobCompletedEvent: {
            double runtime = parsed->now() - job.start_time;
            BAT_ENFORCE(std::abs(runtime - job.expected_runtime) < EPSILON, "job '%s' just finished but had an unexpected runtime (expected=%g, got=%g)", job.job_id.c_str(), job.expected_runtime, runtime);
            register_job_profile = nb_submitted_jobs < nb_jobs_to_submit;
        } break;
        case fb::Event_HostPStateChangedEvent: {
            ++nb_pstate_switches_done;
            if (nb_pstate_switches_done >= job.nb_hosts)
                execute_job = true;
        } break;
        default: break;
        }
    }

    if (register_job_profile) {
        // decide what to submit
        job.nb_hosts = 1 + rng() % (platform_nb_hosts-1);
        job.dvfs_states.clear();
        job.comp.clear();
        for (uint32_t i = 0; i < job.nb_hosts; ++i) {
            job.dvfs_states.push_back(rng() % 2);
            job.comp.push_back(25.0 * (1 + rng() % 10));
        }

        job.profile_id = "dvfs!" + std::to_string(nb_submitted_jobs);
        job.job_id = "dvfs!" + std::to_string(nb_submitted_jobs);
        ++nb_submitted_jobs;

        job.expected_runtime = std::numeric_limits<double>::min();
        for (unsigned int i = 0; i < job.nb_hosts; ++i) {
            double time_on_host_i = job.comp[i] / hosts[i].speeds[job.dvfs_states[i]];
            job.expected_runtime = std::max(job.expected_runtime, time_on_host_i);
        }

        auto prof = batprotocol::Profile::make_parallel_task(
            std::make_shared<std::vector<double> >(std::move(job.comp)),
            nullptr
        );
        mb->add_register_profile(job.profile_id, prof);

        auto j = batprotocol::Job::make();
        j->set_resource_number(job.nb_hosts);
        j->set_profile(job.profile_id);
        mb->add_register_job(job.job_id, j);

        job.alloc = IntervalSet(IntervalSet::ClosedInterval(0, job.nb_hosts-1));

        for (unsigned int i = 0; i < job.nb_hosts; ++i) {
            mb->add_change_host_pstate(std::to_string(job.alloc[i]), job.dvfs_states[i]);
        }
    }

    if (execute_job) {
        job.start_time = parsed->now();
        mb->add_execute_job(job.job_id, job.alloc.to_string_hyphen());
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

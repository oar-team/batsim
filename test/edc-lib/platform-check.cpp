#include <batprotocol.hpp>
#include <cstdint>
#include <filesystem>

#include "batsim_edc.h"

using namespace batprotocol;
namespace fs = std::filesystem;

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used
bool checks_done = false;

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

    BAT_ENFORCE(checks_done, "EDC's deinit function called while platform checks have not been done!");

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
            mb->add_edc_hello("platform-checker", "0.1.0");
        } break;
        case fb::Event_JobSubmittedEvent: {
            auto job_id = event->event_as_JobSubmittedEvent()->job_id()->str();
            mb->add_reject_job(job_id);
        } break;
        case fb::Event_SimulationBeginsEvent: {
            auto sb = event->event_as_SimulationBeginsEvent();

            const std::string expected_workload_filename = "test_delays.json";
            auto wloads = sb->workloads();
            BAT_ENFORCE(wloads->size() == 1, "expected 1 workload but got %u", wloads->size());
            BAT_ENFORCE(wloads->Get(0)->name()->str() == std::string("w0"), "1st workload name is '%s', not w0", wloads->Get(0)->name()->str().c_str());
            const std::string workload_name = wloads->Get(0)->filename()->str();
            const std::string workload_name_basename = fs::path(workload_name).filename().string();
            BAT_ENFORCE(workload_name_basename == expected_workload_filename, "workload filename (%s) is not the expected one (%s)", workload_name.c_str(), expected_workload_filename.c_str());

            const std::string expected_platform_name = "cluster_energy_128.xml";
            bool platform_name_found = false;
            for (unsigned int i = 0; i < sb->batsim_arguments()->size(); ++i) {
                const std::string arg = sb->batsim_arguments()->Get(i)->str();
                const std::string arg_basename = fs::path(arg).filename().string();
                fprintf(stderr, "  batsim arg %u: '%s'\n", i, arg.c_str());
                if (arg_basename == expected_platform_name) {
                    platform_name_found = true;
                    break;
                }
            }
            BAT_ENFORCE(platform_name_found, "did not find the expected platform name '%s' in batsim arguments", expected_platform_name.c_str());

            std::vector<double> expected_computation_speed = {
                100.0e6,
                88.95899053627761e6,
                83.67952522255192e6,
                80.57142857142857e6,
                76.21621621621621e6,
                72.49357326478149e6,
                68.78048780487805e6,
                64.6788990825688e6,
                60.775862068965516e6,
                58.62785862785863e6,
                50.088809946714036e6,
                49.21465968586388e6,
                44.97607655502392e6,
                1e-3,
                0.1639344262295082,
                0.006599788806758183
            };
            auto hosts = sb->computation_hosts();
            BAT_ENFORCE(hosts->size() == 128, "expected 128 hosts but got %u", hosts->size());
            std::vector<std::string> host_names;
            host_names.reserve(hosts->size());
            for (unsigned int i = 0; i < hosts->size(); ++i) {
                auto host = hosts->Get(i);
                BAT_ENFORCE(host->id() == i, "the %u-th computation host has id=%u instead of expected %u", i, host->id(), i);
                const std::string expected_name = "host" + std::to_string(i);
                BAT_ENFORCE(host->name()->str() == expected_name, "host id=%u has name='%s' while '%s' was expected", i, host->name()->str().c_str(), expected_name.c_str());

                BAT_ENFORCE(host->pstate() == 0, "expected pstate=0 for host=%u but got %u", i, host->pstate());
                BAT_ENFORCE(host->pstate_count() == 16, "expected pstate_count=16 for host=%u but got %u", i, host->pstate_count());
                BAT_ENFORCE(host->state() == fb::HostState_IDLE, "host=%u is not in idle state", i);
                BAT_ENFORCE(host->core_count() == 1, "host=%u has %u cores while 1 was expected", i, host->core_count());
                BAT_ENFORCE(host->computation_speed()->size() == 16, "host=%u has %u computation_speeds while 16 were expected", i, host->computation_speed()->size());
                for (unsigned p = 0; p < host->computation_speed()->size(); ++p) {
                    double pstate_speed = host->computation_speed()->Get(p);
                    BAT_ENFORCE(fabs(pstate_speed - expected_computation_speed[p]) < 1e-6, "pstate=%u of host=%u has computation speed of %g, while %g was expected", p, i, pstate_speed, expected_computation_speed[p]);
                }
            }

            checks_done = true;
        } break;
        default: break;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}

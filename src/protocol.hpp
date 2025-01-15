#pragma once

#include <functional>
#include <vector>
#include <string>
#include <map>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <batprotocol.hpp>
#include <intervalset.hpp>

#include "machines.hpp"
#include "workload.hpp"
#include "ipp.hpp"

struct BatsimContext;

namespace protocol
{

std::shared_ptr<batprotocol::KillProgress> battask_to_kill_progress(const BatTask * task);
std::shared_ptr<batprotocol::Job> to_job(const Job & job);
std::shared_ptr<batprotocol::Profile> to_profile(const Profile & profile);

batprotocol::fb::FinalJobState job_state_to_final_job_state(const JobState & state);
batprotocol::SimulationBegins to_simulation_begins(const BatsimContext * context);

ExecuteJobMessage * from_execute_job(const batprotocol::fb::ExecuteJobEvent * execute_job, BatsimContext * context);
RejectJobMessage * from_reject_job(const batprotocol::fb::RejectJobEvent * reject_job, BatsimContext * context);
KillJobsMessage * from_kill_jobs(const batprotocol::fb::KillJobsEvent * kill_jobs, BatsimContext * context);
EDCHelloMessage * from_edc_hello(const batprotocol::fb::EDCHelloEvent * edc_hello, BatsimContext * context);

CallMeLaterMessage * from_call_me_later(const batprotocol::fb::CallMeLaterEvent * call_me_later, BatsimContext * context);
StopCallMeLaterMessage * from_stop_call_me_later(const batprotocol::fb::StopCallMeLaterEvent * stop_call_me_later, BatsimContext * context);
CreateProbeMessage * from_create_probe(const batprotocol::fb::CreateProbeEvent * create_probe, BatsimContext * context);
StopProbeMessage * from_stop_probe(const batprotocol::fb::StopProbeEvent * stop_probe, BatsimContext * context);

JobRegisteredByEDCMessage * from_register_job(const batprotocol::fb::RegisterJobEvent * register_job, BatsimContext * context);
ProfileRegisteredByEDCMessage * from_register_profile(const batprotocol::fb::RegisterProfileEvent * register_profile, BatsimContext * context);

void parse_batprotocol_message(const uint8_t * buffer, uint32_t buffer_size, double & now, std::shared_ptr<std::vector<IPMessageWithTimestamp> > & messages, BatsimContext * context);

} // end of namespace protocol

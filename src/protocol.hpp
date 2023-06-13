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

batprotocol::fb::FinalJobState job_state_to_final_job_state(const JobState & state);
batprotocol::SimulationBegins to_simulation_begins(const BatsimContext * context);

ExecuteJobMessage * from_execute_job(const batprotocol::fb::ExecuteJobEvent * execute_job, BatsimContext * context);
RejectJobMessage * from_reject_job(const batprotocol::fb::RejectJobEvent * reject_job, BatsimContext * context);
KillJobsMessage * from_kill_jobs(const batprotocol::fb::KillJobsEvent * kill_jobs, BatsimContext * context);
ExternalDecisionComponentHelloMessage * from_edc_hello(const batprotocol::fb::ExternalDecisionComponentHelloEvent * edc_hello, BatsimContext * context);

void parse_batprotocol_message(const uint8_t * buffer, uint32_t buffer_size, double & now, std::shared_ptr<std::vector<IPMessageWithTimestamp> > & messages, BatsimContext * context);

} // end of namespace protocol

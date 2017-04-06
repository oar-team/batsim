#include "protocol.hpp"

#include <xbt.h>

void JsonProtocolWriter::append_submit_job(const std::string &job_id,
                                           double date,
                                           const std::string &job_description,
                                           const std::string &profile_description,
                                           bool acknowledge_submission)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_id;
    (void) date;
    (void) job_description;
    (void) profile_description;
    (void) acknowledge_submission;
}

void JsonProtocolWriter::append_execute_job(const std::string &job_id,
                                            const MachineRange &allocated_resources,
                                            double date,
                                            const std::string &executor_to_allocated_resource_mapping)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_id;
    (void) allocated_resources;
    (void) date;
    (void) executor_to_allocated_resource_mapping;
}

void JsonProtocolWriter::append_reject_job(const std::string &job_id,
                                           double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_id;
    (void) date;
}

void JsonProtocolWriter::append_kill_job(const std::vector<std::string> &job_ids,
                                         double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_ids;
    (void) date;
}

void JsonProtocolWriter::append_set_resource_state(MachineRange resources,
                                                   const std::string &new_state,
                                                   double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) resources;
    (void) new_state;
    (void) date;
}

void JsonProtocolWriter::append_call_me_later(double future_date,
                                              double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) future_date;
    (void) date;
}

void JsonProtocolWriter::append_submitter_may_submit_jobs(double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) date;
}

void JsonProtocolWriter::append_scheduler_finished_submitting_jobs(double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) date;
}

void JsonProtocolWriter::append_query_request(void *anything,
                                              double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) anything;
    (void) date;
}



void JsonProtocolWriter::append_simulation_starts(double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) date;
}

void JsonProtocolWriter::append_simulation_ends(double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) date;
}

void JsonProtocolWriter::append_job_submitted(std::vector<std::string> job_ids,
                                              double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) job_ids;
    (void) date;
}

void JsonProtocolWriter::append_job_completed(const std::string &job_id,
                                              double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) job_id;
    (void) date;
}

void JsonProtocolWriter::append_job_killed(std::vector<std::string> job_ids,
                                           double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) job_ids;
    (void) date;
}

void JsonProtocolWriter::append_resource_state_changed(const MachineRange &resources,
                                                       const std::string &new_state,
                                                       double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) resources;
    (void) new_state;
    (void) date;
}

void JsonProtocolWriter::append_query_reply(void *anything,
                                            double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) anything;
    (void) date;
}

void JsonProtocolWriter::clear()
{
    xbt_assert(false, "Unimplemented. TODO!!!");
}

std::string JsonProtocolWriter::generate_current_message(double date)
{
    xbt_assert(false, "Unimplemented. TODO!!!");
    (void) date;
}

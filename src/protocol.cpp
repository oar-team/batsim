#include "protocol.hpp"

#include <xbt.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using namespace rapidjson;
using namespace std;

JsonProtocolWriter::JsonProtocolWriter() :
    _alloc(_doc.GetAllocator())
{
    _doc.SetObject();
}

JsonProtocolWriter::~JsonProtocolWriter()
{

}

void JsonProtocolWriter::append_nop(double date)
{
    xbt_assert(date >= _last_date, "Date inconsistency");
   _last_date = date;
   _is_empty = false;
}

void JsonProtocolWriter::append_submit_job(const string &job_id,
                                           double date,
                                           const string &job_description,
                                           const string &profile_description,
                                           bool acknowledge_submission)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_id;
    (void) date;
    (void) job_description;
    (void) profile_description;
    (void) acknowledge_submission;
}

void JsonProtocolWriter::append_execute_job(const string &job_id,
                                            const MachineRange &allocated_resources,
                                            double date,
                                            const string &executor_to_allocated_resource_mapping)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_id;
    (void) allocated_resources;
    (void) date;
    (void) executor_to_allocated_resource_mapping;
}

void JsonProtocolWriter::append_reject_job(const string &job_id,
                                           double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_id;
    (void) date;
}

void JsonProtocolWriter::append_kill_job(const vector<string> &job_ids,
                                         double date)
{
    xbt_assert(false, "Unimplemented!");
    (void) job_ids;
    (void) date;
}

void JsonProtocolWriter::append_set_resource_state(MachineRange resources,
                                                   const string &new_state,
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
    /* {
      "timestamp": 0.0,
      "type": "SIMULATION_STARTS",
      "data": {}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("SIMULATION_STARTS"), _alloc);
    event.AddMember("data", Value().SetObject(), _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_simulation_ends(double date)
{
    /* {
      "timestamp": 0.0,
      "type": "SIMULATION_ENDS",
      "data": {}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("SIMULATION_ENDS"), _alloc);
    event.AddMember("data", Value().SetObject(), _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_job_submitted(const vector<string> & job_ids,
                                              double date)
{
    /* {
      "timestamp": 10.0,
      "type": "JOB_SUBMITTED",
      "data": {
        "job_ids": ["w0!1", "w0!2"]
      }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_SUBMITTED"), _alloc);

    Value jobs(rapidjson::kArrayType);
    jobs.Reserve(job_ids.size(), _alloc);
    for (const string & job_id : job_ids)
        jobs.PushBack(Value().SetString(job_id.c_str(), _alloc), _alloc);

    event.AddMember("data", Value().SetObject().AddMember("job_ids", jobs, _alloc), _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_job_completed(const string & job_id,
                                              const string & job_status,
                                              double date)
{
    /* {
      "timestamp": 10.0,
      "type": "JOB_COMPLETED",
      "data": {"job_id": "w0!1", "status": "SUCCESS"}
    } */

    const static vector<string> accepted_statuses = {"SUCCESS", "TIMEOUT"};

    xbt_assert(date >= _last_date, "Date inconsistency");
    xbt_assert(std::find(accepted_statuses.begin(), accepted_statuses.end(), job_status) != accepted_statuses.end(),
               "Unsupported job status '%s'!", job_status.c_str());
    _last_date = date;

    Value data(rapidjson::kObjectType);
    data.AddMember("job_id", Value().SetString(job_id.c_str(), _alloc), _alloc);
    data.AddMember("status", Value().SetString(job_status.c_str(), _alloc), _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_COMPLETED"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_job_killed(const vector<string> & job_ids,
                                           double date)
{
    /* {
      "timestamp": 10.0,
      "type": "JOB_KILLED",
      "data": {"job_ids": ["w0!1", "w0!2"]}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_KILLED"), _alloc);

    Value jobs(rapidjson::kArrayType);
    jobs.Reserve(job_ids.size(), _alloc);
    for (const string & job_id : job_ids)
        jobs.PushBack(Value().SetString(job_id.c_str(), _alloc), _alloc);

    event.AddMember("data", Value().SetObject().AddMember("job_ids", jobs, _alloc), _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_resource_state_changed(const MachineRange & resources,
                                                       const string & new_state,
                                                       double date)
{
    /* {
      "timestamp": 10.0,
      "type": "RESOURCE_STATE_CHANGED",
      "data": {"resources": "1 2 3-5", "state": "42"}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;

    Value data(rapidjson::kObjectType);
    data.AddMember("resources",
                   Value().SetString(resources.to_string_hyphen(" ", "-").c_str(), _alloc), _alloc);
    data.AddMember("state", Value().SetString(new_state.c_str(), _alloc), _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("RESOURCE_STATE_CHANGED"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_query_reply_energy(double consumed_energy,
                                                   double date)
{
    /* {
      "timestamp": 10.0,
      "type": "QUERY_REPLY",
      "data": {"energy_consumed": "12500" }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("QUERY_REPLY"), _alloc);
    event.AddMember("data", Value().SetObject().AddMember("energy_consumed", Value().SetDouble(consumed_energy), _alloc), _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::clear()
{
    _is_empty = true;

    _doc.RemoveAllMembers();
    _events.SetArray();
}

string JsonProtocolWriter::generate_current_message(double date)
{
    xbt_assert(date >= _last_date, "Date inconsistency");
    xbt_assert(_events.IsArray(),
               "Successive calls to JsonProtocolWriter::generate_current_message without calling "
               "the clear() method is not supported");

    // Generating the content
    _doc.AddMember("now", Value().SetDouble(date), _alloc);
    _doc.AddMember("events", _events, _alloc);

    // Dumping the content to a buffer
    StringBuffer buffer;
    Writer<rapidjson::StringBuffer> writer(buffer);
    _doc.Accept(writer);

    // Returning the buffer as a string
    return string(buffer.GetString());
}

bool test_json_writer()
{
    AbstractProtocolWriter * proto_writer = new JsonProtocolWriter;
    printf("EMPTY content:\n%s\n", proto_writer->generate_current_message(0).c_str());
    proto_writer->clear();

    proto_writer->append_nop(0);
    printf("NOP content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_simulation_starts(10);
    printf("SIM_START content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_simulation_ends(10);
    printf("SIM_END content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_job_submitted({"w0!j0", "w0!j1"}, 10);
    printf("JOB_SUBMITTED content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_job_completed("w0!j0", "SUCCESS", 10);
    printf("JOB_COMPLETED content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_job_killed({"w0!j0", "w0!j1"}, 10);
    printf("JOB_KILLED content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_resource_state_changed(MachineRange::from_string_hyphen("1,3-5"), "42", 10);
    printf("RESOURCE_STATE_CHANGED content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    proto_writer->append_query_reply_energy(65535, 10);
    printf("QUERY_REPLY (energy) content:\n%s\n", proto_writer->generate_current_message(42).c_str());
    proto_writer->clear();

    delete proto_writer;

    return true;
}

#include "protocol.hpp"

#include <xbt.h>

#include <rapidjson/stringbuffer.h>

#include "context.hpp"
#include "jobs.hpp"
#include "network.hpp"

using namespace rapidjson;
using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(protocol, "protocol"); //!< Logging

JsonProtocolWriter::JsonProtocolWriter(BatsimContext * context) :
    _context(context), _alloc(_doc.GetAllocator())
{
    _doc.SetObject();
}

JsonProtocolWriter::~JsonProtocolWriter()
{

}

void JsonProtocolWriter::append_requested_call(double date)
{
    /* {
      "timestamp": 25.5,
      "type": "REQUESTED_CALL",
      "data": {}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("REQUESTED_CALL"), _alloc);
    event.AddMember("data", Value().SetObject(), _alloc);

    _events.PushBack(event, _alloc);
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



void JsonProtocolWriter::append_simulation_begins(int nb_resources,
                                                  const Document & configuration,
                                                  double date)
{
    /* {
      "timestamp": 0.0,
      "type": "SIMULATION_BEGINS",
      "data": {}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value config(rapidjson::kObjectType);
    config.CopyFrom(configuration, _alloc);

    Value data(rapidjson::kObjectType);
    data.AddMember("nb_resources", Value().SetInt(nb_resources), _alloc);
    data.AddMember("config", config, _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("SIMULATION_BEGINS"), _alloc);
    event.AddMember("data", data, _alloc);

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
    _is_empty = false;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("SIMULATION_ENDS"), _alloc);
    event.AddMember("data", Value().SetObject(), _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_job_submitted(const string & job_id,
                                              const string & job_json_description,
                                              const string & profile_json_description,
                                              double date)
{
    /* "with_redis": {
      "timestamp": 10.0,
      "type": "JOB_SUBMITTED",
      "data": {
        "job_ids": ["w0!1", "w0!2"]
      }
    },
    "without_redis": {
      "timestamp": 10.0,
      "type": "JOB_SUBMITTED",
      "data": {
        "job_id": "dyn!my_new_job",
        "job": {
          "profile": "delay_10s",
          "res": 1,
          "id": "my_new_job",
          "walltime": 12.0
        },
        "profile":{
          "type": "delay",
          "delay": 10
        }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value data(rapidjson::kObjectType);
    data.AddMember("job_id", Value().SetString(job_id.c_str(), _alloc), _alloc);

    if (!_context->redis_enabled)
    {
        Document job_description_doc;
        job_description_doc.Parse(job_json_description.c_str());
        xbt_assert(!job_description_doc.HasParseError());

        data.AddMember("job", Value().CopyFrom(job_description_doc, _alloc), _alloc);

        if (_context->submission_forward_profiles)
        {
            Document profile_description_doc;
            profile_description_doc.Parse(profile_json_description.c_str());
            xbt_assert(!profile_description_doc.HasParseError());

            data.AddMember("profile", Value().CopyFrom(profile_description_doc, _alloc), _alloc);
        }
    }

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_SUBMITTED"), _alloc);
    event.AddMember("data", data, _alloc);

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

    xbt_assert(date >= _last_date, "Date inconsistency");
    xbt_assert(std::find(accepted_completion_statuses.begin(), accepted_completion_statuses.end(), job_status) != accepted_completion_statuses.end(),
               "Unsupported job status '%s'!", job_status.c_str());
    _last_date = date;
    _is_empty = false;

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
    _is_empty = false;

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
    _is_empty = false;

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
    _is_empty = false;

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
    ::Writer<rapidjson::StringBuffer> writer(buffer);
    _doc.Accept(writer);

    // Returning the buffer as a string
    return string(buffer.GetString(), buffer.GetSize());
}



JsonProtocolReader::JsonProtocolReader(BatsimContext *context) :
    context(context)
{
    _type_to_handler_map["QUERY_REQUEST"] = &JsonProtocolReader::handle_query_request;
    _type_to_handler_map["REJECT_JOB"] = &JsonProtocolReader::handle_reject_job;
    _type_to_handler_map["EXECUTE_JOB"] = &JsonProtocolReader::handle_execute_job;
    _type_to_handler_map["CALL_ME_LATER"] = &JsonProtocolReader::handle_call_me_later;
    _type_to_handler_map["KILL_JOB"] = &JsonProtocolReader::handle_kill_job;
    _type_to_handler_map["SUBMIT_JOB"] = &JsonProtocolReader::handle_submit_job;
    _type_to_handler_map["SET_RESOURCE_STATE"] = &JsonProtocolReader::handle_set_resource_state;
    _type_to_handler_map["NOTIFY"] = &JsonProtocolReader::handle_notify;
}

JsonProtocolReader::~JsonProtocolReader()
{
}

void JsonProtocolReader::parse_and_apply_message(const string &message)
{
    rapidjson::Document doc;
    doc.Parse(message.c_str());

    xbt_assert(!doc.HasParseError(), "Invalid JSON message: could not be parsed");
    xbt_assert(doc.IsObject(), "Invalid JSON message: not a JSON object");

    xbt_assert(doc.HasMember("now"), "Invalid JSON message: no 'now' key");
    xbt_assert(doc["now"].IsNumber(), "Invalid JSON message: 'now' value should be a number.");
    double now = doc["now"].GetDouble();

    xbt_assert(doc.HasMember("events"), "Invalid JSON message: no 'events' key");
    const auto & events_array = doc["events"];
    xbt_assert(events_array.IsArray(), "Invalid JSON message: 'events' value should be an array.");

    for (unsigned int i = 0; i < events_array.Size(); ++i)
    {
        const auto & event_object = events_array[i];
        parse_and_apply_event(event_object, i, now);
    }

    send_message(now, "server", IPMessageType::SCHED_READY);
}

void JsonProtocolReader::parse_and_apply_event(const Value & event_object,
                                               int event_number,
                                               double now)
{
    xbt_assert(event_object.IsObject(), "Invalid JSON message: event %d should be an object.", event_number);

    xbt_assert(event_object.HasMember("timestamp"), "Invalid JSON message: event %d should have a 'timestamp' key.", event_number);
    xbt_assert(event_object["timestamp"].IsNumber(), "Invalid JSON message: timestamp of event %d should be a number", event_number);
    double timestamp = event_object["timestamp"].GetDouble();
    xbt_assert(timestamp <= now, "Invalid JSON message: timestamp %g of event %d should be lower than or equal to now=%g.", timestamp, event_number, now);
    (void) now; // Avoids a warning if assertions are ignored

    xbt_assert(event_object.HasMember("type"), "Invalid JSON message: event %d should have a 'type' key.", event_number);
    xbt_assert(event_object["type"].IsString(), "Invalid JSON message: event %d 'type' value should be a String", event_number);
    string type = event_object["type"].GetString();
    xbt_assert(_type_to_handler_map.find(type) != _type_to_handler_map.end(), "Invalid JSON message: event %d has an unknown 'type' value '%s'", event_number, type.c_str());

    xbt_assert(event_object.HasMember("data"), "Invalid JSON message: event %d should have a 'data' key.", event_number);
    const Value & data_object = event_object["data"];

    /*auto handler_function = _type_to_handler_map[type];
    handler_function(this, event_number, timestamp, data_object);*/
    if (type == "QUERY_REQUEST")
        handle_query_request(event_number, timestamp, data_object);
    else if (type == "REJECT_JOB")
        handle_reject_job(event_number, timestamp, data_object);
    else if (type == "EXECUTE_JOB")
        handle_execute_job(event_number, timestamp, data_object);
    else if (type == "CALL_ME_LATER")
        handle_call_me_later(event_number, timestamp, data_object);
    else if (type == "KILL_JOB")
        handle_kill_job(event_number, timestamp, data_object);
    else if (type == "SUBMIT_JOB")
        handle_submit_job(event_number, timestamp, data_object);
    else if (type == "SET_RESOURCE_STATE")
        handle_set_resource_state(event_number, timestamp, data_object);
    else if (type == "NOTIFY")
        handle_notify(event_number, timestamp, data_object);
    else
        xbt_assert(false, "Unknown event!");
}

void JsonProtocolReader::handle_query_request(int event_number, double timestamp, const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "QUERY_REQUEST",
      "data": {
        "requests": {"consumed_energy": {}}
      }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (QUERY_REQUEST) should be an object", event_number);
    xbt_assert(data_object.MemberCount() > 0, "Invalid JSON message: the 'data' value of event %d (QUERY_REQUEST) cannot be empty (size=%d)", event_number, (int)data_object.MemberCount());

    for (auto it = data_object.MemberBegin(); it != data_object.MemberEnd(); ++it)
    {
        const Value & key_value = it->name;
        const Value & value_object = it->value;
        (void) value_object; // Avoids a warning if assertions are ignored

        xbt_assert(key_value.IsString(), "Invalid JSON message: a key within the 'data' object of event %d (QUERY_REQUEST) is not a string", event_number);
        string key = key_value.GetString();
        xbt_assert(std::find(accepted_requests.begin(), accepted_requests.end(), key) != accepted_requests.end(), "Invalid JSON message: Unknown QUERY_REQUEST '%s' of event %d", key.c_str(), event_number);

        xbt_assert(value_object.IsObject(), "Invalid JSON message: the value of '%s' inside 'data' object of event %d (QUERY_REQUEST) is not an object", key.c_str(), event_number);

        if (key == "consumed_energy")
        {
            xbt_assert(value_object.ObjectEmpty(), "Invalid JSON message: the value of '%s' inside 'data' object of event %d (QUERY_REQUEST) should be empty", key.c_str(), event_number);
            send_message(timestamp, "server", IPMessageType::SCHED_TELL_ME_ENERGY);
        }
    }
}

void JsonProtocolReader::handle_reject_job(int event_number,
                                           double timestamp,
                                           const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "REJECT_JOB",
      "data": { "job_id": "w12!45" }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (REJECT_JOB) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 1, "Invalid JSON message: the 'data' value of event %d (REJECT_JOB) should be of size 1 (size=%d)", event_number, (int)data_object.MemberCount());

    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (REJECT_JOB) should contain a 'job_id' key.", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: the 'job_id' value in the 'data' value of event %d (REJECT_JOB) should be a string.", event_number);
    string job_id = job_id_value.GetString();

    JobRejectedMessage * message = new JobRejectedMessage;
    if (!identify_job_from_string(context, job_id, message->job_id))
    {
        xbt_assert(false, "Invalid JSON message: "
                          "Invalid job rejection received: The job identifier '%s' is not valid. "
                          "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                          "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                          "Furthermore, the corresponding job must exist.", job_id.c_str());
    }

    Job * job = context->workloads.job_at(message->job_id);
    (void) job; // Avoids a warning if assertions are ignored
    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
               "Invalid JSON message: "
               "Invalid rejection received: job %s cannot be rejected at the present time."
               "For being rejected, a job must be submitted and not allocated yet.",
               job->id.c_str());

    send_message(timestamp, "server", IPMessageType::SCHED_REJECT_JOB, (void*) message);
}

void JsonProtocolReader::handle_execute_job(int event_number,
                                            double timestamp,
                                            const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "EXECUTE_JOB",
      "data": {
        "job_id": "w12!45",
        "alloc": "2-3",
        "mapping": {"0": "0", "1": "0", "2": "1", "3": "1"}
      }
    } */

    ExecuteJobMessage * message = new ExecuteJobMessage;
    message->allocation = new SchedulingAllocation;

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (EXECUTE_JOB) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 2 || data_object.MemberCount() == 3, "Invalid JSON message: the 'data' value of event %d (EXECUTE_JOB) should be of size in {2,3} (size=%d)", event_number, (int)data_object.MemberCount());

    // *************************
    // Job identifier management
    // *************************
    // Let's read it from the JSON message
    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (EXECUTE_JOB) should contain a 'job_id' key.", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: the 'job_id' value in the 'data' value of event %d (EXECUTE_JOB) should be a string.", event_number);
    string job_id = job_id_value.GetString();

    // Let's retrieve the job identifier
    if (!identify_job_from_string(context, job_id, message->allocation->job_id))
    {
        xbt_assert(false, "Invalid JSON message: in event %d (EXECUTE_JOB): "
                          "The job identifier '%s' is not valid. "
                          "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                          "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                          "Furthermore, the corresponding job must exist.",
                   event_number, job_id.c_str());
    }

    // Let's make sure the job is submitted
    Job * job = context->workloads.job_at(message->allocation->job_id);
    (void) job; // Avoids a warning if assertions are ignored
    xbt_assert(job->state == JobState::JOB_STATE_SUBMITTED,
               "Invalid JSON message: in event %d (EXECUTE_JOB): "
               "Invalid state of job %s ('%d'): It cannot be executed now",
               event_number, job->id.c_str(), (int)job->state);

    // *********************
    // Allocation management
    // *********************
    // Let's read it from the JSON message
    xbt_assert(data_object.HasMember("alloc"), "Invalid JSON message: the 'data' value of event %d (EXECUTE_JOB) should contain a 'alloc' key.", event_number);
    const Value & alloc_value = data_object["alloc"];
    xbt_assert(alloc_value.IsString(), "Invalid JSON message: the 'alloc' value in the 'data' value of event %d (EXECUTE_JOB) should be a string.", event_number);
    string alloc = alloc_value.GetString();

    message->allocation->machine_ids = MachineRange::from_string_hyphen(alloc, " ", "-", "Invalid JSON message: ");
    int nb_allocated_resources = message->allocation->machine_ids.size();
    (void) nb_allocated_resources; // Avoids a warning if assertions are ignored
    xbt_assert(nb_allocated_resources > 0, "Invalid JSON message: in event %d (EXECUTE_JOB): the number of allocated resources should be strictly positive (got %d).", event_number, nb_allocated_resources);

    // *****************************
    // Mapping management (optional)
    // *****************************
    if (data_object.HasMember("mapping"))
    {
        const Value & mapping_value = data_object["mapping"];
        xbt_assert(mapping_value.IsObject(), "Invalid JSON message: the 'mapping' value in the 'data' value of event %d (EXECUTE_JOB) should be a string.", event_number);
        xbt_assert(mapping_value.MemberCount() > 0, "Invalid JSON: the 'mapping' value in the 'data' value of event %d (EXECUTE_JOB) must be a non-empty object", event_number);
        map<int,int> mapping_map;

        // Let's fill the map from the JSON description
        for (auto it = mapping_value.MemberBegin(); it != mapping_value.MemberEnd(); ++it)
        {
            const Value & key_value = it->name;
            const Value & value_value = it->value;

            xbt_assert(key_value.IsInt() || key_value.IsString(), "Invalid JSON message: Invalid 'mapping' of event %d (EXECUTE_JOB): a key is not an integer nor a string", event_number);
            xbt_assert(value_value.IsInt() || value_value.IsString(), "Invalid JSON message: Invalid 'mapping' of event %d (EXECUTE_JOB): a value is not an integer nor a string", event_number);

            int executor;
            int resource;

            try
            {
                if (key_value.IsInt())
                    executor = key_value.GetInt();
                else
                    executor = std::stoi(key_value.GetString());

                if (value_value.IsInt())
                    resource = value_value.GetInt();
                else
                    resource = std::stoi(key_value.GetString());
            }
            catch (const std::exception & e)
            {
                xbt_assert(false, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): all keys and values must be integers (or strings representing integers)", event_number);
                throw e;
            }

            mapping_map[executor] = resource;
        }

        // Let's write the mapping as a vector (keys will be implicit between 0 and nb_executor-1)
        message->allocation->mapping.reserve(mapping_map.size());
        auto mit = mapping_map.begin();
        int nb_inserted = 0;

        xbt_assert(mit->first == nb_inserted, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): no resource associated to executor %d.", event_number, nb_inserted);
        xbt_assert(mit->second >= 0 && mit->second < nb_allocated_resources, "Invalid JSON message: Invalid 'mapping' object of evend %d (EXECUTE_JOB): the %d-th resource within the allocation is told to be used, but there are only %d allocated resources.", event_number, mit->second, nb_allocated_resources);
        message->allocation->mapping.push_back(mit->second);

        for (++mit, ++nb_inserted; mit != mapping_map.end(); ++mit, ++nb_inserted)
        {
            xbt_assert(mit->first == nb_inserted, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): no resource associated to executor %d.", event_number, nb_inserted);
            xbt_assert(mit->second >= 0 && mit->second < nb_allocated_resources, "Invalid JSON message: Invalid 'mapping' object of evend %d (EXECUTE_JOB): the %d-th resource within the allocation is told to be used, but there are only %d allocated resources.", event_number, mit->second, nb_allocated_resources);
            message->allocation->mapping.push_back(mit->second);
        }

        xbt_assert(message->allocation->mapping.size() == mapping_map.size());
    }

    // Everything has been parsed correctly, let's inject the message into the simulation.
    send_message(timestamp, "server", IPMessageType::SCHED_EXECUTE_JOB, (void*) message);
}

void JsonProtocolReader::handle_call_me_later(int event_number,
                                              double timestamp,
                                              const Value &data_object)
{
    /* {
      "timestamp": 10.0,
      "type": "CALL_ME_LATER",
      "data": {"timestamp": 25.5}
    } */

    CallMeLaterMessage * message = new CallMeLaterMessage;

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (CALL_ME_LATER) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 1, "Invalid JSON message: the 'data' value of event %d (CALL_ME_LATER) should be of size 1 (size=%d)", event_number, (int)data_object.MemberCount());

    xbt_assert(data_object.HasMember("timestamp"), "Invalid JSON message: the 'data' value of event %d (CALL_ME_LATER) should contain a 'timestamp' key.", event_number);
    const Value & timestamp_value = data_object["timestamp"];
    xbt_assert(timestamp_value.IsNumber(), "Invalid JSON message: the 'timestamp' value in the 'data' value of event %d (CALL_ME_LATER) should be a number.", event_number);
    message->target_time = timestamp_value.GetDouble();

    if (message->target_time < MSG_get_clock())
        XBT_WARN("Event %d (CALL_ME_LATER) asks to be called at time %g but it is already reached", event_number, message->target_time);

    send_message(timestamp, "server", IPMessageType::SCHED_CALL_ME_LATER, (void*) message);
}

void JsonProtocolReader::handle_set_resource_state(int event_number,
                                                   double timestamp,
                                                   const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "SET_RESOURCE_STATE",
      "data": {"resources": "1 2 3-5", "state": "42"}
    } */
    PStateModificationMessage * message = new PStateModificationMessage;

    // ********************
    // Resources management
    // ********************
    // Let's read it from the JSON message
    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 2, "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should be of size 2 (size=%d)", event_number, (int)data_object.MemberCount());

    xbt_assert(data_object.HasMember("resources"), "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should contain a 'resources' key.", event_number);
    const Value & resources_value = data_object["resources"];
    xbt_assert(resources_value.IsString(), "Invalid JSON message: the 'resources' value in the 'data' value of event %d (SET_RESOURCE_STATE) should be a string.", event_number);
    string resources = resources_value.GetString();

    message->machine_ids = MachineRange::from_string_hyphen(resources, " ", "-", "Invalid JSON message: ");
    int nb_allocated_resources = message->machine_ids.size();
    (void) nb_allocated_resources; // Avoids a warning if assertions are ignored
    xbt_assert(nb_allocated_resources > 0, "Invalid JSON message: in event %d (SET_RESOURCE_STATE): the number of allocated resources should be strictly positive (got %d).", event_number, nb_allocated_resources);

    // State management
    xbt_assert(data_object.HasMember("state"), "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should contain a 'state' key.", event_number);
    const Value & state_value = data_object["state"];
    xbt_assert(state_value.IsString(), "Invalid JSON message: the 'state' value in the 'data' value of event %d (SET_RESOURCE_STATE) should be a string.", event_number);
    string state_value_string = state_value.GetString();
    try { message->new_pstate = std::stoi(state_value_string); }
    catch(const std::exception & e)
    {
        xbt_assert(false, "Invalid JSON message: the 'state' value in the 'data' value of event %d (SET_RESOURCE_STATE) should be a string corresponding to an integer (got '%s')", event_number, state_value_string.c_str());
        throw e;
    }

    send_message(timestamp, "server", IPMessageType::PSTATE_MODIFICATION, (void*) message);
}

void JsonProtocolReader::handle_notify(int event_number,
                                       double timestamp,
                                       const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 42.0,
      "type": "NOTIFY",
      "data": { "type": "submission_finished" }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (NOTIFY) should be an object", event_number);

    xbt_assert(data_object.HasMember("type"), "Invalid JSON message: the 'data' value of event %d (NOTIFY) should have a 'type' key", event_number);
    const Value & notify_type_value = data_object["type"];
    xbt_assert(notify_type_value.IsString(), "Invalid JSON message: in event %d (NOTIFY): ['data']['type'] should be a string", event_number);
    string notify_type = notify_type_value.GetString();

    if (notify_type == "submission_finished")
        send_message(timestamp, "server", IPMessageType::END_DYNAMIC_SUBMIT);
    else
        xbt_assert(false, "Unknown NOTIFY type received ('%s').", notify_type.c_str());

    (void) timestamp;
}

void JsonProtocolReader::handle_submit_job(int event_number,
                                           double timestamp,
                                           const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* "with_redis": {
      "timestamp": 10.0,
      "type": "SUBMIT_JOB",
      "data": {
        "job_id": "w12!45",
      }
    },
    "without_redis": {
      "timestamp": 10.0,
      "type": "SUBMIT_JOB",
      "data": {
        "job_id": "dyn!my_new_job",
        "job":{
          "profile": "delay_10s",
          "res": 1,
          "id": "my_new_job",
          "walltime": 12.0
        },
        "profile":{
          "type": "delay",
          "delay": 10
        }
      }
    } */

    JobSubmittedByDPMessage * message = new JobSubmittedByDPMessage;

    xbt_assert(context->submission_sched_enabled, "Invalid JSON message: dynamic job submission received but the option seems disabled...");

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (SUBMIT_JOB) should be an object", event_number);

    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (SUBMIT_JOB) should have a 'job_id' key", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['job_id'] should be a string", event_number);
    string job_id = job_id_value.GetString();

    if (!identify_job_from_string(context, job_id, message->job_id, false))
        xbt_assert(false, "Invalid JSON message: in event %d (SUBMIT_JOB): job_id '%s' seems invalid (already exists?)", event_number, job_id.c_str());

    static int job_received = 0;
    int buf_size = 128;

    char * buf_job = new char[buf_size];
    int nb_chars = snprintf(buf_job, buf_size,
             R"foo({"id":%d, "subtime":%g, "walltime":%g, "res":%d, "profile":"%s"})foo",
             job_received, MSG_get_clock(), 50.0, 1, "bouh");
    xbt_assert(nb_chars < buf_size - 1);

    message->job_description = buf_job;
    delete[] buf_job;

    char * buf_profile = new char[buf_size];
    nb_chars = snprintf(buf_profile, buf_size,
            R"foo({"type": "delay", "delay": %g})foo", 42.0);
    xbt_assert(nb_chars < buf_size - 1);

    message->job_profile_description = buf_profile;
    delete[] buf_profile;

    ++job_received;

    if (data_object.HasMember("job"))
    {
        xbt_assert(!context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): 'job' object is given but redis seems disabled...", event_number);

        const Value & job_object = data_object["job"];
        xbt_assert(job_object.IsObject(), "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['job'] should be an object", event_number);

        /*StringBuffer buffer;
        ::Writer<rapidjson::StringBuffer> writer(buffer);
        job_object.Accept(writer);

        message->job_description = string(buffer.GetString(), buffer.GetSize());*/
    }
    else
        xbt_assert(context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['job'] is unset but redis seems enabled...", event_number);

    if (data_object.HasMember("profile"))
    {
        xbt_assert(!context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): 'profile' object is given but redis seems disabled...", event_number);

        const Value & profile_object = data_object["profile"];
        xbt_assert(profile_object.IsObject(), "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['profile'] should be an object", event_number);

        /*StringBuffer buffer;
        ::Writer<rapidjson::StringBuffer> writer(buffer);
        profile_object.Accept(writer);

        message->job_profile_description = string(buffer.GetString(), buffer.GetSize());*/
    }
    else
        xbt_assert(context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['profile'] is unset but redis seems enabled...", event_number);

    send_message(timestamp, "server", IPMessageType::JOB_SUBMITTED_BY_DP, (void *) message);
}

void JsonProtocolReader::handle_kill_job(int event_number,
                                         double timestamp,
                                         const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "KILL_JOB",
      "data": {"job_ids": ["w0!1", "w0!2"]}
    } */

    KillJobMessage * message = new KillJobMessage;

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (KILL_JOB) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 1, "Invalid JSON message: the 'data' value of event %d (KILL_JOB) should be of size 1 (size=%d)", event_number, (int)data_object.MemberCount());

    xbt_assert(data_object.HasMember("job_ids"), "Invalid JSON message: the 'data' value of event %d (KILL_JOB) should contain a 'job_ids' key.", event_number);
    const Value & job_ids_array = data_object["job_ids"];
    xbt_assert(job_ids_array.IsArray(), "Invalid JSON message: the 'job_ids' value in the 'data' value of event %d (KILL_JOB) should be an array.", event_number);
    xbt_assert(job_ids_array.Size() > 0, "Invalid JSON message: the 'job_ids' array in the 'data' value of event %d (KILL_JOB) should be non-empty.", event_number);
    message->jobs_ids.resize(job_ids_array.Size());

    for (unsigned int i = 0; i < job_ids_array.Size(); ++i)
    {
        const Value & job_id_value = job_ids_array[i];
        if (!identify_job_from_string(context, job_id_value.GetString(), message->jobs_ids[i]))
            xbt_assert(false, "Invalid JSON message: in event %d (KILL_JOB): job_id %d ('%s') is invalid.", event_number, i, message->jobs_ids[i].to_string().c_str());
    }

    send_message(timestamp, "server", IPMessageType::SCHED_KILL_JOB, (void *) message);
}

void JsonProtocolReader::send_message(double when,
                                      const string &destination_mailbox,
                                      IPMessageType type,
                                      void *data,
                                      bool detached) const
{
    // Let's wait until "when" time is reached
    double current_time = MSG_get_clock();
    if (when > current_time)
        MSG_process_sleep(when - current_time);

    // Let's actually send the message
    generic_send_message(destination_mailbox, type, data, detached);
}

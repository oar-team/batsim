#include "protocol.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/regex.hpp>

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

void JsonProtocolWriter::append_simulation_begins(Machines & machines,
                                                  Workloads & workloads,
                                                  const Document & configuration,
                                                  bool allow_time_sharing,
                                                  double date)
{
    /*{
      "timestamp": 0.0,
      "type": "SIMULATION_BEGINS",
      "data": {
        "allow_time_sharing": false,
        "nb_resources": 1,
        "config": {},
        "resources_data": [
          {
            "id": 0,
            "name": "host0",
            "state": "idle",
            "properties": {}
          }
        ],
        "workloads": {
          "1s3fa1f5d1": "workload0.json",
          "41d51f8d4f": "workload1.json",
        }
      }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value config(rapidjson::kObjectType);
    config.CopyFrom(configuration, _alloc);

    Value data(rapidjson::kObjectType);
    data.AddMember("nb_resources", Value().SetInt(machines.nb_machines()), _alloc);
    data.AddMember("allow_time_sharing", Value().SetBool(allow_time_sharing), _alloc);
    data.AddMember("config", config, _alloc);

    Value resources(rapidjson::kArrayType);
    resources.Reserve(machines.nb_machines(), _alloc);
    for (const Machine * machine : machines.machines())
    {
        resources.PushBack(machine_to_json_value(*machine), _alloc);
    }
    data.AddMember("resources_data", Value().CopyFrom(resources, _alloc), _alloc);

    if (machines.has_hpst_machine())
    {
        data.AddMember("hpst_host", machine_to_json_value(*machines.hpst_machine()), _alloc);
    }

    if (machines.has_pfs_machine())
    {
        data.AddMember("lcst_host", machine_to_json_value(*machines.pfs_machine()), _alloc);
    }

    Value workloads_dict(rapidjson::kObjectType);
    for (const auto & workload : workloads.workloads())
    {
        workloads_dict.AddMember(
            Value().SetString(workload.first.c_str(), _alloc),
            Value().SetString(workload.second->file.c_str(), _alloc),
            _alloc);
    }
    data.AddMember("workloads", workloads_dict, _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("SIMULATION_BEGINS"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

Value JsonProtocolWriter::machine_to_json_value(const Machine & machine)
{
    Value machine_doc(rapidjson::kObjectType);
    machine_doc.AddMember("id", Value().SetInt(machine.id), _alloc);
    machine_doc.AddMember("name", Value().SetString(machine.name.c_str(), _alloc), _alloc);
    machine_doc.AddMember("state", Value().SetString(machine_state_to_string(machine.state).c_str(), _alloc), _alloc);

    Value properties(rapidjson::kObjectType);
    for(auto const &entry : machine.properties)
    {
        rapidjson::Value key(entry.first.c_str(), _alloc);
        rapidjson::Value value(entry.second.c_str(), _alloc);
        properties.AddMember(key, value, _alloc);
    }
    machine_doc.AddMember("properties", properties, _alloc);

    return machine_doc;
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
                                              const string & job_state,
                                              const string & kill_reason,
                                              const string & job_alloc,
                                              int return_code,
                                              double date)
{
    /* {
      "timestamp": 10.0,
      "type": "JOB_COMPLETED",
      "data": {
        "job_id": "w0!1",
        "status": "SUCCESS",
        "job_state": "COMPLETED_KILLED",
        "kill_reason": "Walltime reached"
        "job_alloc": "0-1 5"
      }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    xbt_assert(std::find(accepted_completion_statuses.begin(), accepted_completion_statuses.end(), job_status) != accepted_completion_statuses.end(),
               "Unsupported job status '%s'!", job_status.c_str());
    _last_date = date;
    _is_empty = false;

    Value data(rapidjson::kObjectType);
    data.AddMember("job_id", Value().SetString(job_id.c_str(), _alloc), _alloc);
    data.AddMember("status", Value().SetString(job_status.c_str(), _alloc), _alloc);
    data.AddMember("job_state", Value().SetString(job_state.c_str(), _alloc), _alloc);
    data.AddMember("return_code", Value().SetInt(return_code), _alloc);
    data.AddMember("kill_reason", Value().SetString(kill_reason.c_str(), _alloc), _alloc);
    data.AddMember("alloc", Value().SetString(job_alloc.c_str(), _alloc), _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_COMPLETED"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

/**
 * @brief Create task tree with progress in Json and add it to _alloc
 */
Value generate_task_tree(BatTask* task_tree, rapidjson::Document::AllocatorType & _alloc)
{
    Value task(rapidjson::kObjectType);
    // add final task (leaf) progress
    if (task_tree->ptask != nullptr || task_tree->delay_task_start != -1)
    {
        task.AddMember("profile", Value().SetString(task_tree->profile->name.c_str(), _alloc), _alloc);
        task.AddMember("progress", Value().SetDouble(task_tree->current_task_progress_ratio), _alloc);
    }
    else
    {
        task.AddMember("profile", Value().SetString(task_tree->profile->name.c_str(), _alloc), _alloc);
        task.AddMember("current_task_index", Value().SetInt(task_tree->current_task_index), _alloc);

        BatTask * btask = task_tree->sub_tasks[task_tree->current_task_index];
        task.AddMember("current_task", generate_task_tree(btask, _alloc), _alloc);
    }
    return task;
}

void JsonProtocolWriter::append_job_killed(const vector<string> & job_ids,
                                           const std::map<string, BatTask *> & job_progress,
                                           double date)
{
    /*
    {
      "timestamp": 10.0,
      "type": "JOB_KILLED",
      "data": {
        "job_ids": ["w0!1", "w0!2"],
        "job_progress":
          {
          // simple job
          "w0!1": {"profile": "my_simple_profile", "progress": 0.52},
          // sequential job
          "w0!2":
          {
            "profile": "my_sequential_profile",
            "current_task_index": 3,
            "current_task":
            {
              "profile": "my_simple_profile",
              "progress": 0.52
            }
          },
          // composed sequential job
          "w0!3:
          {
            "profile": "my_composed_profile",
            "current_task_index": 2,
            "current_task":
            {
              "profile": "my_sequential_profile",
              "current_task_index": 3,
              "current_task":
              {
                "profile": "my_simple_profile",
                "progress": 0.52
              }
            }
          },
        }
      }
    }
    */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_KILLED"), _alloc);

    Value jobs(rapidjson::kArrayType);
    jobs.Reserve(job_ids.size(), _alloc);

    Value progress(rapidjson::kObjectType);

    for (const string& job_id : job_ids)
    {
        jobs.PushBack(Value().SetString(job_id.c_str(), _alloc), _alloc);
        // compute task progress tree
        if (job_progress.at(job_id) != nullptr) {
            progress.AddMember(Value().SetString(job_id.c_str(), _alloc),
                generate_task_tree(job_progress.at(job_id), _alloc), _alloc);
        }
    }

    Value data(rapidjson::kObjectType);
    data.AddMember("job_ids", jobs, _alloc);
    data.AddMember("job_progress", progress, _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_from_job_message(const string & job_id,
                                                 const Document & message,
                                                 double date)
{
    /* {
      "timestamp": 10.0,
      "type": "FROM_JOB_MSG",
      "data": {
            "job_id": "w0!1",
            "msg": "some_message"
      }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value data(rapidjson::kObjectType);
    data.AddMember("job_id",
                   Value().SetString(job_id.c_str(), _alloc), _alloc);
    data.AddMember("msg", Value().CopyFrom(message, _alloc), _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("FROM_JOB_MSG"), _alloc);
    event.AddMember("data", data, _alloc);

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

void JsonProtocolWriter::append_query_estimate_waiting_time(const string &job_id,
                                                            const string &job_json_description,
                                                            double date)
{
    /* {
      "timestamp": 10.0,
      "type": "QUERY",
      "data": {
        "requests": {
          "estimate_waiting_time": {
            "job_id": "workflow_submitter0!potential_job17",
            "job": {
              "res": 1,
              "walltime": 12.0
            }
          }
        }
      }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value estimate_object(rapidjson::kObjectType);

    Document job_description_doc;
    job_description_doc.Parse(job_json_description.c_str());
    xbt_assert(!job_description_doc.HasParseError());
    estimate_object.AddMember("job_id", Value().SetString(job_id.c_str(), _alloc), _alloc);
    estimate_object.AddMember("job", Value().CopyFrom(job_description_doc, _alloc), _alloc);

    Value requests_object(rapidjson::kObjectType);
    requests_object.AddMember("estimate_waiting_time", estimate_object, _alloc);

    Value data(rapidjson::kObjectType);
    data.AddMember("requests", requests_object, _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("QUERY"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_answer_energy(double consumed_energy,
                                              double date)
{
    /* {
      "timestamp": 10.0,
      "type": "ANSWER",
      "data": {"consumed_energy": 12500.0}
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("ANSWER"), _alloc);
    event.AddMember("data", Value().SetObject().AddMember("consumed_energy", Value().SetDouble(consumed_energy), _alloc), _alloc);

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
    _type_to_handler_map["QUERY"] = &JsonProtocolReader::handle_query;
    _type_to_handler_map["ANSWER"] = &JsonProtocolReader::handle_answer;
    _type_to_handler_map["REJECT_JOB"] = &JsonProtocolReader::handle_reject_job;
    _type_to_handler_map["EXECUTE_JOB"] = &JsonProtocolReader::handle_execute_job;
    _type_to_handler_map["CHANGE_JOB_STATE"] = &JsonProtocolReader::handle_change_job_state;
    _type_to_handler_map["CALL_ME_LATER"] = &JsonProtocolReader::handle_call_me_later;
    _type_to_handler_map["KILL_JOB"] = &JsonProtocolReader::handle_kill_job;
    _type_to_handler_map["SUBMIT_JOB"] = &JsonProtocolReader::handle_submit_job;
    _type_to_handler_map["SUBMIT_PROFILE"] = &JsonProtocolReader::handle_submit_profile;
    _type_to_handler_map["SET_RESOURCE_STATE"] = &JsonProtocolReader::handle_set_resource_state;
    _type_to_handler_map["SET_JOB_METADATA"] = &JsonProtocolReader::handle_set_job_metadata;
    _type_to_handler_map["NOTIFY"] = &JsonProtocolReader::handle_notify;
    _type_to_handler_map["TO_JOB_MSG"] = &JsonProtocolReader::handle_to_job_msg;
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

    auto handler_function = _type_to_handler_map[type];
    handler_function(this, event_number, timestamp, data_object);
}

void JsonProtocolReader::handle_query(int event_number, double timestamp, const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "QUERY",
      "data": {
        "requests": {"consumed_energy": {}}
      }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (QUERY) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 1, "Invalid JSON message: the 'data' value of event %d (QUERY) must be of size 1 (size=%d)", event_number, (int)data_object.MemberCount());
    xbt_assert(data_object.HasMember("requests"), "Invalid JSON message: the 'data' value of event %d (QUERY) must have a 'requests' member", event_number);

    const Value & requests = data_object["requests"];
    xbt_assert(requests.IsObject(), "Invalid JSON message: the 'requests' member of the 'data' object  of event %d (QUERY) must be an object", event_number);
    xbt_assert(requests.MemberCount() > 0, "Invalid JSON message: the 'requests' object of the 'data' object of event %d (QUERY) must be non-empty", event_number);

    for (auto it = requests.MemberBegin(); it != requests.MemberEnd(); ++it)
    {
        const Value & key_value = it->name;
        const Value & value_object = it->value;
        (void) value_object; // Avoids a warning if assertions are ignored

        xbt_assert(key_value.IsString(), "Invalid JSON message: a key within the 'data' object of event %d (QUERY) is not a string", event_number);
        string key = key_value.GetString();
        xbt_assert(std::find(accepted_requests.begin(), accepted_requests.end(), key) != accepted_requests.end(), "Invalid JSON message: Unknown QUERY '%s' of event %d", key.c_str(), event_number);

        xbt_assert(value_object.IsObject(), "Invalid JSON message: the value of '%s' inside the 'requests' object of the 'data' object of event %d (QUERY) is not an object", key.c_str(), event_number);

        if (key == "consumed_energy")
        {
            xbt_assert(value_object.ObjectEmpty(), "Invalid JSON message: the value of '%s' inside the 'requests' object of the 'data' object of event %d (QUERY) should be empty", key.c_str(), event_number);
            send_message(timestamp, "server", IPMessageType::SCHED_TELL_ME_ENERGY);
        }
        else
        {
            xbt_assert(0, "Invalid JSON message: in event %d (QUERY): request type '%s' is unknown", event_number, key.c_str());
        }
    }
}

void JsonProtocolReader::handle_answer(int event_number,
                                       double timestamp,
                                       const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 10.0,
      "type": "ANSWER",
      "data": {
        "estimate_waiting_time": {
          "job_id": "workflow_submitter0!potential_job17",
          "estimated_waiting_time": 56
        }
      }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (ANSWER) should be an object", event_number);
    xbt_assert(data_object.MemberCount() > 0, "Invalid JSON message: the 'data' object of event %d (ANSWER) must be non-empty (size=%d)", event_number, (int)data_object.MemberCount());

    for (auto it = data_object.MemberBegin(); it != data_object.MemberEnd(); ++it)
    {
        string key_value = it->name.GetString();
        const Value & value_object = it->value;

        if (key_value == "estimate_waiting_time")
        {
            xbt_assert(value_object.IsObject(), "Invalid JSON message: the value of the '%s' key of event %d (ANSWER) should be an object", key_value.c_str(), event_number);

            xbt_assert(value_object.HasMember("job_id"), "Invalid JSON message: the object of '%s' key of event %d (ANSWER) should have a 'job_id' field", key_value.c_str(), event_number);
            const Value & job_id_value = value_object["job_id"];
            xbt_assert(job_id_value.IsString(), "Invalid JSON message: the value of the 'job_id' field (on the '%s' key) of event %d should be a string", key_value.c_str(), event_number);
            string job_id = job_id_value.GetString();

            xbt_assert(value_object.HasMember("estimated_waiting_time"), "Invalid JSON message: the object of '%s' key of event %d (ANSWER) should have a 'estimated_waiting_time' field", key_value.c_str(), event_number);
            const Value & estimated_waiting_time_value = value_object["estimated_waiting_time"];
            xbt_assert(estimated_waiting_time_value.IsNumber(), "Invalid JSON message: the value of the 'estimated_waiting_time' field (on the '%s' key) of event %d should be a number", key_value.c_str(), event_number);
            double estimated_waiting_time = estimated_waiting_time_value.GetDouble();

            XBT_WARN("Received an ANSWER of type 'estimate_waiting_time' with job_id='%s' and 'estimated_waiting_time'=%g. "
                     "However, I do not know what I should do with it.",
                     job_id.c_str(), estimated_waiting_time);
            (void) timestamp;
        }
        else
        {
            xbt_assert(0, "Invalid JSON message: unknown ANSWER type '%s' in event %d", key_value.c_str(), event_number);
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
               job->id.to_string().c_str());

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
    if (!identify_job_from_string(context, job_id, message->allocation->job_id,
                                  IdentifyJobReturnCondition::STRING_VALID))
    {
        xbt_assert(false, "Invalid JSON message: in event %d (EXECUTE_JOB): "
                          "The job identifier '%s' is not valid. "
                          "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                          "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                          "Furthermore, the corresponding job must exist.",
                   event_number, job_id.c_str());
    }

    // *********************
    // Allocation management
    // *********************
    // Let's read it from the JSON message
    xbt_assert(data_object.HasMember("alloc"), "Invalid JSON message: the 'data' value of event %d (EXECUTE_JOB) should contain a 'alloc' key.", event_number);
    const Value & alloc_value = data_object["alloc"];
    xbt_assert(alloc_value.IsString(), "Invalid JSON message: the 'alloc' value in the 'data' value of event %d (EXECUTE_JOB) should be a string.", event_number);
    string alloc = alloc_value.GetString();

    message->allocation->machine_ids = MachineRange::from_string_hyphen(alloc, " ", "-", "Invalid JSON message received from the scheduler");
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
                {
                    executor = key_value.GetInt();
                }
                else
                {
                    executor = std::stoi(key_value.GetString());
                }

                if (value_value.IsInt())
                {
                    resource = value_value.GetInt();
                }
                else
                {
                    resource = std::stoi(value_value.GetString());
                }
            }
            catch (const std::exception &)
            {
                xbt_assert(false, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): all keys and values must be integers (or strings representing integers)", event_number);
                throw;
            }

            mapping_map[executor] = resource;
        }

        // Let's write the mapping as a vector (keys will be implicit between 0 and nb_executor-1)
        message->allocation->mapping.reserve(mapping_map.size());
        auto mit = mapping_map.begin();
        int nb_inserted = 0;

        xbt_assert(mit->first == nb_inserted, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): no resource associated to executor %d.", event_number, nb_inserted);
        xbt_assert(mit->second >= 0 && mit->second < nb_allocated_resources, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): executor %d should use the %d-th resource within the allocation, but there are only %d allocated resources.", event_number, mit->first, mit->second, nb_allocated_resources);
        message->allocation->mapping.push_back(mit->second);

        for (++mit, ++nb_inserted; mit != mapping_map.end(); ++mit, ++nb_inserted)
        {
            xbt_assert(mit->first == nb_inserted, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): no resource associated to executor %d.", event_number, nb_inserted);
            xbt_assert(mit->second >= 0 && mit->second < nb_allocated_resources, "Invalid JSON message: Invalid 'mapping' object of event %d (EXECUTE_JOB): executor %d should use the %d-th resource within the allocation, but there are only %d allocated resources.", event_number, mit->first, mit->second, nb_allocated_resources);
            message->allocation->mapping.push_back(mit->second);
        }

        xbt_assert(message->allocation->mapping.size() == mapping_map.size());
    }
    else
    {
        // Default mapping
        message->allocation->mapping.resize(nb_allocated_resources);
        for (int i = 0; i < nb_allocated_resources; ++i)
        {
            message->allocation->mapping[i] = i;
        }
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
    {
        XBT_WARN("Event %d (CALL_ME_LATER) asks to be called at time %g but it is already reached", event_number, message->target_time);
    }

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

    message->machine_ids = MachineRange::from_string_hyphen(resources, " ", "-", "Invalid JSON message received from the scheduler");
    int nb_allocated_resources = message->machine_ids.size();
    (void) nb_allocated_resources; // Avoids a warning if assertions are ignored
    xbt_assert(nb_allocated_resources > 0, "Invalid JSON message: in event %d (SET_RESOURCE_STATE): the number of allocated resources should be strictly positive (got %d).", event_number, nb_allocated_resources);

    // State management
    xbt_assert(data_object.HasMember("state"), "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should contain a 'state' key.", event_number);
    const Value & state_value = data_object["state"];
    xbt_assert(state_value.IsString(), "Invalid JSON message: the 'state' value in the 'data' value of event %d (SET_RESOURCE_STATE) should be a string.", event_number);
    string state_value_string = state_value.GetString();
    try
    {
        message->new_pstate = std::stoi(state_value_string);
    }
    catch(const std::exception &)
    {
        xbt_assert(false, "Invalid JSON message: the 'state' value in the 'data' value of event %d (SET_RESOURCE_STATE) should be a string corresponding to an integer (got '%s')", event_number, state_value_string.c_str());
        throw;
    }

    send_message(timestamp, "server", IPMessageType::PSTATE_MODIFICATION, (void*) message);
}

void JsonProtocolReader::handle_set_job_metadata(int event_number,
                                                 double timestamp,
                                                 const Value & data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    (void) timestamp;
    /* {
      "timestamp": 13.0,
      "type": "SET_JOB_METADATA",
      "data": {
        "job_id": "wload!42",
        "metadata": "scheduler-defined string"
      }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (SET_JOB_METADATA) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 2, "Invalid JSON message: the 'data' value of event %d (SET_JOB_METADATA) should be of size 2 (size=%d)", event_number, (int)data_object.MemberCount());

    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (SET_JOB_METADATA) should have a 'job_id' key", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: in event %d (SET_JOB_METADATA): ['data']['job_id'] should be a string", event_number);
    string job_id = job_id_value.GetString();

    xbt_assert(data_object.HasMember("metadata"), "Invalid JSON message: the 'data' value of event %d (SET_JOB_METADATA) should contain a 'metadata' key.", event_number);
    const Value & metadata_value = data_object["metadata"];
    xbt_assert(metadata_value.IsString(), "Invalid JSON message: the 'metadata' value in the 'data' value of event %d (SET_JOB_METADATA) should be a string.", event_number);
    string metadata = metadata_value.GetString();

    // Check metadata validity regarding CSV output
    boost::regex r("[^\"]*");
    xbt_assert(boost::regex_match(metadata, r), "Invalid JSON message: the 'metadata' value in the 'data' value of event %d (SET_JOB_METADATA) should not contain double quotes (got ###%s###)", event_number, metadata.c_str());

    JobIdentifier job_identifier;
    if (!identify_job_from_string(context, job_id, job_identifier))
    {
        xbt_assert(false, "Invalid JSON message: "
                          "Invalid job change job state received: The job identifier '%s' is not valid. "
                          "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                          "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                          "Furthermore, the corresponding job must exist.", job_id.c_str());
    }

    Job * job = context->workloads.job_at(job_identifier);
    job->metadata = metadata;
}

void JsonProtocolReader::handle_change_job_state(int event_number,
                                       double timestamp,
                                       const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 42.0,
      "type": "CHANGE_JOB_STATE",
      "data": {
            "job_id": "w12!45",
            "job_state": "COMPLETED_KILLED",
            "kill_reason": "Sub-jobs were killed."
      }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (CHANGE_JOB_STATE) should be an object", event_number);

    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (CHANGE_JOB_STATE) should have a 'job_id' key", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: in event %d (CHANGE_JOB_STATE): ['data']['job_id'] should be a string", event_number);
    string job_id = job_id_value.GetString();

    xbt_assert(data_object.HasMember("job_state"), "Invalid JSON message: the 'data' value of event %d (CHANGE_JOB_STATE) should have a 'job_state' key", event_number);
    const Value & job_state_value = data_object["job_state"];
    xbt_assert(job_state_value.IsString(), "Invalid JSON message: in event %d (CHANGE_JOB_STATE): ['data']['job_state'] should be a string", event_number);
    string job_state = job_state_value.GetString();

    set<string> allowed_states = {"NOT_SUBMITTED",
                                  "RUNNING",
                                  "COMPLETED_SUCCESSFULLY",
                                  "COMPLETED_WALLTIME_REACHED",
                                  "COMPLETED_KILLED",
                                  "REJECTED"};


    if (allowed_states.count(job_state) != 1)
    {
        xbt_assert(false, "Invalid JSON message: in event %d (CHANGE_JOB_STATE): "
                   "['data']['job_state'] must be one of: {%s}", event_number,
                   boost::algorithm::join(allowed_states, ", ").c_str());
    }

    string kill_reason;
    if (data_object.HasMember("kill_reason"))
    {
        const Value & kill_reason_value = data_object["kill_reason"];
        xbt_assert(kill_reason_value.IsString(), "Invalid JSON message: in event %d (CHANGE_kill_reason): ['data']['kill_reason'] should be a string", event_number);
        kill_reason = kill_reason_value.GetString();

        if (kill_reason != "" && job_state != "COMPLETED_KILLED")
        {
            xbt_assert(false, "Invalid JSON message: in event %d (CHANGE_JOB_STATE): ['data']['kill_reason'] is only allowed if the job_state is COMPLETED_KILLED", event_number);
        }
    }

    ChangeJobStateMessage * message = new ChangeJobStateMessage;

    if (!identify_job_from_string(context, job_id, message->job_id))
    {
        xbt_assert(false, "Invalid JSON message: "
                          "Invalid job change job state received: The job identifier '%s' is not valid. "
                          "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                          "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                          "Furthermore, the corresponding job must exist.", job_id.c_str());
    }

    message->job_state = job_state;
    message->kill_reason = kill_reason;

    send_message(timestamp, "server", IPMessageType::SCHED_CHANGE_JOB_STATE, (void *) message);
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
    {
        send_message(timestamp, "server", IPMessageType::END_DYNAMIC_SUBMIT);
    }
    else if (notify_type == "continue_submission")
    {
        send_message(timestamp, "server", IPMessageType::CONTINUE_DYNAMIC_SUBMIT);
    }
    else
    {
        xbt_assert(false, "Unknown NOTIFY type received ('%s').", notify_type.c_str());
    }

    (void) timestamp;
}

void JsonProtocolReader::handle_to_job_msg(int event_number,
                                       double timestamp,
                                       const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 42.0,
      "type": "TO_JOB_MSG",
      "data": {
            "job_id": "w!0",
            "msg": "Some answer"
      }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (TO_JOB_MSG) should be an object", event_number);

    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (TO_JOB_MSG) should have a 'job_id' key", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: in event %d (TO_JOB_MSG): ['data']['job_id'] should be a string", event_number);
    string job_id = job_id_value.GetString();

    xbt_assert(data_object.HasMember("msg"), "Invalid JSON msg: the 'data' value of event %d (TO_JOB_MSG) should have a 'msg' key", event_number);
    const Value & msg_value = data_object["msg"];
    xbt_assert(msg_value.IsString(), "Invalid JSON msg: in event %d (TO_JOB_MSG): ['data']['msg'] should be a string", event_number);
    string msg = msg_value.GetString();

    ToJobMessage * message = new ToJobMessage;

    if (!identify_job_from_string(context, job_id, message->job_id))
    {
        xbt_assert(false, "Invalid JSON message: "
                          "Invalid job change job state received: The job identifier '%s' is not valid. "
                          "Job identifiers must be of the form [WORKLOAD_NAME!]JOB_ID. "
                          "If WORKLOAD_NAME! is omitted, WORKLOAD_NAME='static' is used. "
                          "Furthermore, the corresponding job must exist.", job_id.c_str());
    }
    message->message = msg;

    send_message(timestamp, "server", IPMessageType::TO_JOB_MSG, (void *) message);
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

    if (!identify_job_from_string(context, job_id, message->job_id,
                                  IdentifyJobReturnCondition::STRING_VALID__JOB_DOES_NOT_EXISTS))
    {
        xbt_assert(false, "Invalid JSON message: in event %d (SUBMIT_JOB): job_id '%s' seems invalid (already exists?)", event_number, job_id.c_str());
    }

    if (data_object.HasMember("job"))
    {
        xbt_assert(!context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): 'job' object is given but redis seems disabled...", event_number);

        const Value & job_object = data_object["job"];
        xbt_assert(job_object.IsObject(), "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['job'] should be an object", event_number);

        StringBuffer buffer;
        ::Writer<rapidjson::StringBuffer> writer(buffer);
        job_object.Accept(writer);

        message->job_description = string(buffer.GetString(), buffer.GetSize());
    }
    else
    {
        xbt_assert(context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['job'] is unset but redis seems enabled...", event_number);
    }

    if (data_object.HasMember("profile"))
    {
        xbt_assert(!context->redis_enabled, "Invalid JSON message: in event %d (SUBMIT_JOB): 'profile' object is given but redis seems disabled...", event_number);

        const Value & profile_object = data_object["profile"];
        xbt_assert(profile_object.IsObject(), "Invalid JSON message: in event %d (SUBMIT_JOB): ['data']['profile'] should be an object", event_number);

        StringBuffer buffer;
        ::Writer<rapidjson::StringBuffer> writer(buffer);
        profile_object.Accept(writer);

        message->job_profile_description = string(buffer.GetString(), buffer.GetSize());
    }

    // Let's put the job into memory now (so that next events at the same time stamp can refer to this job).
    xbt_assert(context->submission_sched_enabled,
               "Job submission coming from the decision process received but the option "
               "seems disabled... It can be activated by specifying a configuration file "
               "to Batsim.");

    xbt_assert(!context->workloads.job_exists(message->job_id),
               "Bad job submission received from the decision process: job %s already exists.",
               message->job_id.to_string().c_str());

    // Let's create the workload if it doesn't exist, or retrieve it otherwise
    Workload * workload = nullptr;
    if (context->workloads.exists(message->job_id.workload_name))
    {
        workload = context->workloads.at(message->job_id.workload_name);
    }
    else
    {
        workload = new Workload(message->job_id.workload_name, "Dynamic");
        context->workloads.insert_workload(workload->name, workload);
    }

    // If redis is enabled, the job description must be retrieved from it
    if (context->redis_enabled)
    {
        xbt_assert(message->job_description.empty(), "Internal error");
        string job_key = RedisStorage::job_key(message->job_id);
        message->job_description = context->storage.get(job_key);
    }
    else
    {
        xbt_assert(!message->job_description.empty(), "Internal error");
    }

    // Let's parse the user-submitted job
    XBT_INFO("Parsing user-submitted job %s", message->job_id.to_string().c_str());
    Job * job = Job::from_json(message->job_description, workload,
                               "Invalid JSON job submitted by the scheduler");
    workload->jobs->add_job(job);
    job->id = JobIdentifier(workload->name, job->number);

    // Let's parse the profile if needed
    if (!workload->profiles->exists(job->profile))
    {
        XBT_INFO("The profile of user-submitted job '%s' does not exist yet.",
                 job->profile.c_str());

        // If redis is enabled, the profile description must be retrieved from it
        if (context->redis_enabled)
        {
            xbt_assert(message->job_profile_description.empty(), "Internal error");
            string profile_key = RedisStorage::profile_key(message->job_id.workload_name,
                                                           job->profile);
            message->job_profile_description = context->storage.get(profile_key);
        }
        else
        {
            xbt_assert(!message->job_profile_description.empty(), "Internal error");
        }

        Profile * profile = Profile::from_json(job->profile,
                                               message->job_profile_description,
                                               "Invalid JSON profile received from the scheduler");
        workload->profiles->add_profile(job->profile, profile);
    }
    // TODO? check profile collisions otherwise

    // Let's set the job state
    job->state = JobState::JOB_STATE_SUBMITTED;

    send_message(timestamp, "server", IPMessageType::JOB_SUBMITTED_BY_DP, (void *) message);
}

void JsonProtocolReader::handle_submit_profile(int event_number,
                                           double timestamp,
                                           const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* "with_redis": {
      "timestamp": 10.0,
      "type": "SUBMIT_PROFILE",
      "data": {
        "workload_name": "w12",
        "profile_name": "delay.0.1",
        "profile": {
          "type": "delay",
          "delay": 10
        }
      }
    } */
    ProfileSubmittedByDPMessage * message = new ProfileSubmittedByDPMessage;

    xbt_assert(context->submission_sched_enabled, "Invalid JSON message: dynamic profile submission received but the option seems disabled...");

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (SUBMIT_JOB) should be an object", event_number);

    xbt_assert(data_object.HasMember("workload_name"), "Invalid JSON message: the 'data' value of event %d (SUBMIT_PROFILE) should have a 'workload_name' key", event_number);
    const Value & workload_name_value = data_object["workload_name"];
    xbt_assert(workload_name_value.IsString(), "Invalid JSON message: in event %d (SUBMIT_PROFILE): ['data']['workload_name'] should be a string", event_number);
    string workload_name = workload_name_value.GetString();

    xbt_assert(data_object.HasMember("profile_name"), "Invalid JSON message: the 'data' value of event %d (SUBMIT_PROFILE) should have a 'profile_name' key", event_number);
    const Value & profile_name_value = data_object["profile_name"];
    xbt_assert(profile_name_value.IsString(), "Invalid JSON message: in event %d (SUBMIT_PROFILE): ['data']['profile_name'] should be a string", event_number);
    string profile_name = profile_name_value.GetString();

    xbt_assert(data_object.HasMember("profile"), "Invalid JSON message: the 'data' value of event %d (SUBMIT_PROFILE) should have a 'profile' key", event_number);

    const Value & profile_object = data_object["profile"];
    xbt_assert(profile_object.IsObject(), "Invalid JSON message: in event %d (SUBMIT_PROFILE): ['data']['profile'] should be an object", event_number);

    message->workload_name = workload_name;
    message->profile_name = profile_name;

    StringBuffer buffer;
    ::Writer<rapidjson::StringBuffer> writer(buffer);
    profile_object.Accept(writer);

    message->profile = string(buffer.GetString(), buffer.GetSize());

    send_message(timestamp, "server", IPMessageType::PROFILE_SUBMITTED_BY_DP, (void *) message);
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
        {
            xbt_assert(false, "Invalid JSON message: in event %d (KILL_JOB): job_id %d ('%s') is invalid.", event_number, i, message->jobs_ids[i].to_string().c_str());
        }
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
    {
        MSG_process_sleep(when - current_time);
    }

    // Let's actually send the message
    generic_send_message(destination_mailbox, type, data, detached);
}

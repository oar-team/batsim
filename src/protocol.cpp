#include "protocol.hpp"

#include <regex>

#include <boost/algorithm/string/join.hpp>

#include <xbt.h>

#include <rapidjson/stringbuffer.h>

#include "batsim.hpp"
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
                                                  bool allow_compute_sharing,
                                                  bool allow_storage_sharing,
                                                  double date)
{
    /*{
      "timestamp": 0.0,
      "type": "SIMULATION_BEGINS",
      "data": {
        "allow_compute_sharing": false,
        "allow_storage_sharing": true,
        "nb_compute_resources": 1,
        "nb_storage_resources": 1,
        "config": {},
        "compute_resources": [
          {
            "id": 0,
            "name": "host0",
            "state": "idle",
            "properties": {}
          }
        ],
        "storage_resources": [
          {
            "id": 2,
            "name": "host2",
            "state": "idle",
            "properties": {"roles": "storage"}
          }
        ],
        "workloads": {
          "1s3fa1f5d1": "workload0.json",
          "41d51f8d4f": "workload1.json",
        }
        "profiles": {
          "1s3fa1f5d1": {
            "delay_10s": {
              "type": "delay",
              "delay": 10
            },
            "compute_only_10s":{
              "type":"parallel_homogeneous",
              "cpu": 1e9,
              "com": 0
            }
          }
          "41d51f8d4f": {
            "no_com": {
              "com": 0,
              "type": "parallel_homogeneous",
              "cpu": 1000000000.0
            }
          }
        }
      }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value config(rapidjson::kObjectType);
    config.CopyFrom(configuration, _alloc);

    Value data(rapidjson::kObjectType);
    data.AddMember("nb_resources", Value().SetInt(static_cast<int>(machines.nb_machines())), _alloc);
    data.AddMember("nb_compute_resources", Value().SetInt(static_cast<int>(machines.nb_compute_machines())), _alloc);
    data.AddMember("nb_storage_resources", Value().SetInt(static_cast<int>(machines.nb_storage_machines())), _alloc);
    // FIXME this should be in the configuration and not there
    data.AddMember("allow_compute_sharing", Value().SetBool(allow_compute_sharing), _alloc);
    data.AddMember("allow_storage_sharing", Value().SetBool(allow_storage_sharing), _alloc);
    data.AddMember("config", config, _alloc);

    Value compute_resources(rapidjson::kArrayType);
    compute_resources.Reserve(machines.nb_compute_machines(), _alloc);
    for (const Machine * machine : machines.compute_machines())
    {
        compute_resources.PushBack(machine_to_json_value(*machine), _alloc);
    }
    data.AddMember("compute_resources", Value().CopyFrom(compute_resources, _alloc), _alloc);
    Value storage_resources(rapidjson::kArrayType);
    storage_resources.Reserve(machines.nb_storage_machines(), _alloc);
    for (const Machine * machine : machines.storage_machines())
    {
        storage_resources.PushBack(machine_to_json_value(*machine), _alloc);
    }
    data.AddMember("storage_resources", Value().CopyFrom(storage_resources, _alloc), _alloc);


    Value workloads_dict(rapidjson::kObjectType);
    Value profiles_dict(rapidjson::kObjectType);
    for (const auto & workload : workloads.workloads())
    {
        workloads_dict.AddMember(
            Value().SetString(workload.first.c_str(), _alloc),
            Value().SetString(workload.second->file.c_str(), _alloc),
            _alloc);

        Value profile_dict(rapidjson::kObjectType);
        for (const auto & profile : workload.second->profiles->profiles())
        {
            if (profile.second.get() != nullptr) // unused profiles may have been removed from memory at workload loading time.
            {
                Document profile_description_doc;
                const string & profile_json_description = profile.second->json_description;
                profile_description_doc.Parse(profile_json_description.c_str());
                profile_dict.AddMember(
                        Value().SetString(profile.first.c_str(), _alloc),
                        Value().CopyFrom(profile_description_doc, _alloc), _alloc);
            }
        }
        profiles_dict.AddMember(
                Value().SetString(workload.first.c_str(), _alloc),
                profile_dict, _alloc);
    }
    data.AddMember("workloads", workloads_dict, _alloc);
    data.AddMember("profiles", profiles_dict, _alloc);

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

    Value zone_properties(rapidjson::kObjectType);
    for(auto const &entry : machine.zone_properties)
    {
        rapidjson::Value key(entry.first.c_str(), _alloc);
        rapidjson::Value value(entry.second.c_str(), _alloc);
        zone_properties.AddMember(key, value, _alloc);
    }
    machine_doc.AddMember("zone_properties", zone_properties, _alloc);

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
    /* "without_redis": {
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

    Document job_description_doc;
    job_description_doc.Parse(job_json_description.c_str());
    xbt_assert(!job_description_doc.HasParseError(), "JSON parse error");

    data.AddMember("job", Value().CopyFrom(job_description_doc, _alloc), _alloc);

    if (_context->submission_forward_profiles)
    {
        Document profile_description_doc;
        profile_description_doc.Parse(profile_json_description.c_str());
        xbt_assert(!profile_description_doc.HasParseError(), "JSON parse error");

        data.AddMember("profile", Value().CopyFrom(profile_description_doc, _alloc), _alloc);
    }

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("JOB_SUBMITTED"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_job_completed(const string & job_id,
                                              const string & job_state,
                                              const string & job_alloc,
                                              int return_code,
                                              double date)
{
    /* {
      "timestamp": 10.0,
      "type": "JOB_COMPLETED",
      "data": {
        "job_id": "w0!1",
        "job_state": "COMPLETED_KILLED",
        "alloc": "0-1 5"
        "return_code": -1
      }
    }
    or {
      "timestamp": 15.0,
      "type": "JOB_COMPLETED",
      "data": {
        "job_id": "w0!2",
        "job_state": "COMPLETED_SUCCESSFULLY",
        "alloc": "0-3"
        "return_code": 0
      }
    }*/

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value data(rapidjson::kObjectType);
    data.AddMember("job_id", Value().SetString(job_id.c_str(), _alloc), _alloc);
    data.AddMember("job_state", Value().SetString(job_state.c_str(), _alloc), _alloc);
    data.AddMember("return_code", Value().SetInt(return_code), _alloc);
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
        task.AddMember("profile_name", Value().SetString(task_tree->profile->name.c_str(), _alloc), _alloc);
        task.AddMember("progress", Value().SetDouble(task_tree->current_task_progress_ratio), _alloc);
    }
    else
    {
        task.AddMember("profile_name", Value().SetString(task_tree->profile->name.c_str(), _alloc), _alloc);

        if (task_tree->current_task_index != static_cast<unsigned int>(-1)) // Started parallel task
        {
            task.AddMember("current_task_index", Value().SetInt(static_cast<int>(task_tree->current_task_index)), _alloc);

            BatTask * btask = task_tree->sub_tasks[task_tree->current_task_index];
            task.AddMember("current_task", generate_task_tree(btask, _alloc), _alloc);
        }
        else
        {
            task.AddMember("current_task_index", Value().SetInt(-1), _alloc);
            XBT_WARN("Cannot generate the execution task tree of job %s, "
                     "as its execution has not started.",
                     static_cast<JobPtr>(task_tree->parent_job)->id.to_string().c_str());
        }
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
    jobs.Reserve(static_cast<unsigned int>(job_ids.size()), _alloc);

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

void JsonProtocolWriter::append_resource_state_changed(const IntervalSet & resources,
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
    xbt_assert(!job_description_doc.HasParseError(), "JSON parse error");
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

void JsonProtocolWriter::append_notify(const std::string & notify_type,
                                       double date)
{
    /* {
       "timestamp": 23.57,
       "type": "NOTIFY",
       "data": { "type": "no_more_static_job_to_submit" }
    }
    or {
       "timestamp": 23.57,
       "type": "NOTIFY",
       "data": { "type": "no_more_external_event_to_occur" }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value data(rapidjson::kObjectType);
    data.AddMember("type", Value().SetString(notify_type.c_str(), _alloc), _alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("NOTIFY"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_notify_resource_event(const std::string & notify_type,
                                          const IntervalSet & resources,
                                          double date)
{
    /* {
        "timestamp": 140.0,
        "type": "NOTIFY",
        "data": { "type": "event_resource_available", "resources": "0 5-8" }
    }
    or {
        "timestamp": 200.0,
        "type": "NOTIFY",
        "data": { "type": "event_resource_unavailable", "resources": "0 5 7" }
    } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value data(rapidjson::kObjectType);
    data.AddMember("type", Value().SetString(notify_type.c_str(), _alloc),_alloc);
    data.AddMember("resources", Value().SetString(resources.to_string_hyphen(" ", "-").c_str(), _alloc),_alloc);

    Value event(rapidjson::kObjectType);
    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("NOTIFY"), _alloc);
    event.AddMember("data", data, _alloc);

    _events.PushBack(event, _alloc);
}

void JsonProtocolWriter::append_notify_generic_event(const std::string & json_desc_str,
                                                     double date)
{
    /* {
        "timestamp" : 12.3,
        "type": "NOTIFY",
        "data": // A JSON object representing an external event
      } */

    xbt_assert(date >= _last_date, "Date inconsistency");
    _last_date = date;
    _is_empty = false;

    Value event(rapidjson::kObjectType);

    event.AddMember("timestamp", Value().SetDouble(date), _alloc);
    event.AddMember("type", Value().SetString("NOTIFY"), _alloc);

    Document event_doc;
    event_doc.Parse(json_desc_str.c_str());
    xbt_assert(!event_doc.HasParseError(), "JSON parse error");
    event.AddMember("data", Value().CopyFrom(event_doc, _alloc), _alloc);


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
    _type_to_handler_map["CHANGE_JOB_STATE"] = &JsonProtocolReader::handle_change_job_state;
    _type_to_handler_map["CALL_ME_LATER"] = &JsonProtocolReader::handle_call_me_later;
    _type_to_handler_map["REGISTER_JOB"] = &JsonProtocolReader::handle_register_job;
    _type_to_handler_map["REGISTER_PROFILE"] = &JsonProtocolReader::handle_register_profile;
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
        parse_and_apply_event(event_object, static_cast<int>(i), now);
    }

    send_message_at_time(now, "server", IPMessageType::SCHED_READY);
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
    XBT_DEBUG("Starting event processing (number: %d, Type: %s)", event_number, type.c_str());

    handler_function(this, event_number, timestamp, data_object);
    XBT_DEBUG("Finished event processing (number: %d, Type: %s)", event_number, type.c_str());
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
    xbt_assert(data_object.MemberCount() == 1, "Invalid JSON message: the 'data' value of event %d (QUERY) must be of size 1 (size=%d)", event_number, data_object.MemberCount());
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
            send_message_at_time(timestamp, "server", IPMessageType::SCHED_TELL_ME_ENERGY);
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
    xbt_assert(data_object.MemberCount() > 0, "Invalid JSON message: the 'data' object of event %d (ANSWER) must be non-empty (size=%d)", event_number, data_object.MemberCount());

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

void JsonProtocolReader::handle_call_me_later(int event_number,
                                              double timestamp,
                                              const Value &data_object)
{
    /* {
      "timestamp": 10.0,
      "type": "CALL_ME_LATER",
      "data": {"timestamp": 25.5}
    } */

    auto * message = new CallMeLaterMessage;

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (CALL_ME_LATER) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 1, "Invalid JSON message: the 'data' value of event %d (CALL_ME_LATER) should be of size 1 (size=%d)", event_number, data_object.MemberCount());

    xbt_assert(data_object.HasMember("timestamp"), "Invalid JSON message: the 'data' value of event %d (CALL_ME_LATER) should contain a 'timestamp' key.", event_number);
    const Value & timestamp_value = data_object["timestamp"];
    xbt_assert(timestamp_value.IsNumber(), "Invalid JSON message: the 'timestamp' value in the 'data' value of event %d (CALL_ME_LATER) should be a number.", event_number);
    message->target_time = timestamp_value.GetDouble();

    if (message->target_time < simgrid::s4u::Engine::get_clock())
    {
        XBT_WARN("Event %d (CALL_ME_LATER) asks to be called at time %g but it is already reached", event_number, message->target_time);
    }

    send_message_at_time(timestamp, "server", IPMessageType::SCHED_CALL_ME_LATER, static_cast<void*>(message));
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
    auto * message = new PStateModificationMessage;

    // ********************
    // Resources management
    // ********************
    // Let's read it from the JSON message
    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should be an object", event_number);
    xbt_assert(data_object.MemberCount() == 2, "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should be of size 2 (size=%d)", event_number, data_object.MemberCount());

    xbt_assert(data_object.HasMember("resources"), "Invalid JSON message: the 'data' value of event %d (SET_RESOURCE_STATE) should contain a 'resources' key.", event_number);
    const Value & resources_value = data_object["resources"];
    xbt_assert(resources_value.IsString(), "Invalid JSON message: the 'resources' value in the 'data' value of event %d (SET_RESOURCE_STATE) should be a string.", event_number);
    string resources = resources_value.GetString();

    try { message->machine_ids = IntervalSet::from_string_hyphen(resources, " ", "-"); }
    catch(const std::exception & e) { throw std::runtime_error(std::string("Invalid JSON message: ") + e.what());}

    int nb_allocated_resources = static_cast<int>(message->machine_ids.size());
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

    send_message_at_time(timestamp, "server", IPMessageType::PSTATE_MODIFICATION, static_cast<void*>(message));
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

    // Put this as a 'global' set?
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

    auto * message = new ChangeJobStateMessage;
    message->job_id = JobIdentifier(job_id);
    message->job_state = job_state;

    send_message_at_time(timestamp, "server", IPMessageType::SCHED_CHANGE_JOB_STATE, static_cast<void*>(message));
}

void JsonProtocolReader::handle_notify(int event_number,
                                       double timestamp,
                                       const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* {
      "timestamp": 42.0,
      "type": "NOTIFY",
      "data": { "type": "registration_finished" }
    } */

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (NOTIFY) should be an object", event_number);

    xbt_assert(data_object.HasMember("type"), "Invalid JSON message: the 'data' value of event %d (NOTIFY) should have a 'type' key", event_number);
    const Value & notify_type_value = data_object["type"];
    xbt_assert(notify_type_value.IsString(), "Invalid JSON message: in event %d (NOTIFY): ['data']['type'] should be a string", event_number);
    string notify_type = notify_type_value.GetString();

    if (notify_type == "registration_finished")
    {
        send_message_at_time(timestamp, "server", IPMessageType::END_DYNAMIC_REGISTER);
    }
    else
    {
        xbt_assert(false, "Unknown NOTIFY type received ('%s').", notify_type.c_str());
    }

    (void) timestamp;
}

void JsonProtocolReader::handle_register_job(int event_number,
                                           double timestamp,
                                           const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* "without_redis": {
      "timestamp": 10.0,
      "type": "REGISTER_JOB",
      "data": {
        "job_id": "dyn!my_new_job",
        "job":{
          "profile": "delay_10s",
          "res": 1,
          "id": "my_new_job",
          "walltime": 12.0
        }
      }
    } */

    auto * message = new JobRegisteredByDPMessage;

    xbt_assert(context->registration_sched_enabled, "Invalid JSON message: dynamic job registration received but the option seems disabled... "
                                                  "It can be activated with the '--enable-dynamic-jobs' command line option.");

    xbt_assert(!context->registration_sched_finished, "Invalid JSON message: dynamic job registration received but the option has been disabled (a registration_finished message have already been received)");

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (REGISTER_JOB) should be an object", event_number);

    xbt_assert(data_object.HasMember("job_id"), "Invalid JSON message: the 'data' value of event %d (REGISTER_JOB) should have a 'job_id' key", event_number);
    const Value & job_id_value = data_object["job_id"];
    xbt_assert(job_id_value.IsString(), "Invalid JSON message: in event %d (REGISTER_JOB): ['data']['job_id'] should be a string", event_number);
    string job_id_str = job_id_value.GetString();
    JobIdentifier job_id(job_id_str);

    // Read the job description
    xbt_assert(data_object.HasMember("job"), "Invalid JSON message: in event %d (REGISTER_JOB): 'job' object is missing", event_number);

    const Value & job_object = data_object["job"];
    xbt_assert(job_object.IsObject(), "Invalid JSON message: in event %d (REGISTER_JOB): ['data']['job'] should be an object", event_number);

    StringBuffer buffer;
    ::Writer<rapidjson::StringBuffer> writer(buffer);
    job_object.Accept(writer);

    message->job_description = string(buffer.GetString(), buffer.GetSize());

    // Load job into memory. TODO: this should be between the protocol parsing and the injection in the events, not here.
    xbt_assert(context->workloads.exists(job_id.workload_name()),
               "Internal error: Workload '%s' should exist.",
               job_id.workload_name().c_str());
    xbt_assert(!context->workloads.job_is_registered(job_id),
               "Cannot register new job '%s', it already exists in the workload.", job_id.to_cstring());

    Workload * workload = context->workloads.at(job_id.workload_name());

    // Create the job.
    XBT_DEBUG("Parsing user-submitted job %s", job_id.to_cstring());
    message->job = Job::from_json(message->job_description, workload, "Invalid JSON job submitted by the scheduler");
    xbt_assert(message->job->id.job_name() == job_id.job_name(), "Internal error");
    xbt_assert(message->job->id.workload_name() == job_id.workload_name(), "Internal error");

    /* The check of existence of a profile is done in Job::from_json which should raise an Exception
     * TODO catch this exception here and print the following message
     * if (!workload->profiles->exists(job->profile))
    {
        xbt_die(
                   "Dynamically registered job '%s' has no profile: "
                   "Workload '%s' has no profile named '%s'. "
                   "When registering a dynamic job, its profile should already exist. "
                   "If the profile is also dynamic, it can be registered with the REGISTER_PROFILE "
                   "message but you must ensure that the profile is sent (non-strictly) before "
                   "the REGISTER_JOB message.",
                   job->id.to_cstring(),
                   workload->name.c_str(), job->profile.c_str());
    }*/

    workload->check_single_job_validity(message->job);
    workload->jobs->add_job(message->job);
    message->job->state = JobState::JOB_STATE_SUBMITTED;

    send_message_at_time(timestamp, "server", IPMessageType::JOB_REGISTERED_BY_DP, static_cast<void*>(message));
}

void JsonProtocolReader::handle_register_profile(int event_number,
                                           double timestamp,
                                           const Value &data_object)
{
    (void) event_number; // Avoids a warning if assertions are ignored
    /* "with_redis": {
      "timestamp": 10.0,
      "type": "REGISTER_PROFILE",
      "data": {
        "workload_name": "w12",
        "profile_name": "delay.0.1",
        "profile": {
          "type": "delay",
          "delay": 10
        }
      }
    } */

    // Read message
    auto * message = new ProfileRegisteredByDPMessage;

    xbt_assert(context->registration_sched_enabled, "Invalid JSON message: dynamic profile registration received but the option seems disabled... "
                                                  "It can be activated with the '--enable-dynamic-jobs' command line option.");

    xbt_assert(!context->registration_sched_finished, "Invalid JSON message: dynamic profile registration received but the option has been disabled (a registration_finished message have already been received)");

    xbt_assert(data_object.IsObject(), "Invalid JSON message: the 'data' value of event %d (REGISTER_PROFILE) should be an object", event_number);

    xbt_assert(data_object.HasMember("workload_name"), "Invalid JSON message: the 'data' value of event %d (REGISTER_PROFILE) should have a 'workload_name' key", event_number);
    const Value & workload_name_value = data_object["workload_name"];
    xbt_assert(workload_name_value.IsString(), "Invalid JSON message: in event %d (REGISTER_PROFILE): ['data']['workload_name'] should be a string", event_number);
    string workload_name = workload_name_value.GetString();

    xbt_assert(data_object.HasMember("profile_name"), "Invalid JSON message: the 'data' value of event %d (REGISTER_PROFILE) should have a 'profile_name' key", event_number);
    const Value & profile_name_value = data_object["profile_name"];
    xbt_assert(profile_name_value.IsString(), "Invalid JSON message: in event %d (REGISTER_PROFILE): ['data']['profile_name'] should be a string", event_number);
    string profile_name = profile_name_value.GetString();

    xbt_assert(data_object.HasMember("profile"), "Invalid JSON message: the 'data' value of event %d (REGISTER_PROFILE) should have a 'profile' key", event_number);

    const Value & profile_object = data_object["profile"];
    xbt_assert(profile_object.IsObject(), "Invalid JSON message: in event %d (REGISTER_PROFILE): ['data']['profile'] should be an object", event_number);

    message->workload_name = workload_name;
    message->profile_name = profile_name;

    StringBuffer buffer;
    ::Writer<rapidjson::StringBuffer> writer(buffer);
    profile_object.Accept(writer);

    message->profile = string(buffer.GetString(), buffer.GetSize());

    // Load profile into memory. TODO: this should be between the protocol parsing and the injection in the events, not here.

    // Retrieve the workload, or create if it does not exist yet
    Workload * workload = nullptr;
    if (context->workloads.exists(message->workload_name))
    {
        workload = context->workloads.at(message->workload_name);
    }
    else
    {
        workload = Workload::new_dynamic_workload(message->workload_name);
        context->workloads.insert_workload(workload->name, workload);
    }

    if (!workload->profiles->exists(message->profile_name))
    {
        XBT_INFO("Adding dynamically registered profile %s to workload %s",
                message->profile_name.c_str(),
                message->workload_name.c_str());
        auto profile = Profile::from_json(message->profile_name,
                                          message->profile,
                                          "Invalid JSON profile received from the scheduler");
        workload->profiles->add_profile(message->profile_name, profile);
    }
    else
    {
        xbt_die("Invalid new profile registration: profile '%s' already existed in workload '%s'",
            message->profile_name.c_str(),
            message->workload_name.c_str());
    }

    send_message_at_time(timestamp, "server", IPMessageType::PROFILE_REGISTERED_BY_DP, static_cast<void*>(message));
}

void JsonProtocolReader::send_message_at_time(double when,
                                      const string &destination_mailbox,
                                      IPMessageType type,
                                      void *data,
                                      bool detached) const
{
    // Let's wait until "when" time is reached
    double current_time = simgrid::s4u::Engine::get_clock();
    if (when > current_time)
    {
        simgrid::s4u::this_actor::sleep_for(when - current_time);
    }

    // Let's actually send the message
    generic_send_message(destination_mailbox, type, data, detached);
}

namespace protocol
{

/**
 * @brief Computes the KillProgress of a BatTask
 * @param[in] The task whose kill progress must be computed
 * @return The KillProgress of the given BatTask
 */
std::shared_ptr<batprotocol::KillProgress> battask_to_kill_progress(const BatTask * task)
{
    auto kp = batprotocol::KillProgress::make(task->unique_name());

    std::stack<const BatTask *> tasks;
    tasks.push(task);

    while (!tasks.empty())
    {
        auto t = tasks.top();
        tasks.pop();

        switch (t->profile->type)
        {
        case ProfileType::PTASK: // missing breaks are not a mistake.
        case ProfileType::PTASK_HOMOGENEOUS:
        case ProfileType::PTASK_ON_STORAGE_HOMOGENEOUS:
        case ProfileType::PTASK_DATA_STAGING_BETWEEN_STORAGES:
        { // Profile is a parallel task.
            double task_progress_ratio = 0;
            if (t->ptask != nullptr) // The parallel task has already started
            {
                // WARNING: 'get_remaining_ratio' is not returning the flops amount but the remaining quantity of work
                // from 1 (not started yet) to 0 (completely finished)
                task_progress_ratio = 1 - t->ptask->get_remaining_ratio();
            }
            kp->add_atomic(t->unique_name(), t->profile->name, task_progress_ratio);
        } break;
        case ProfileType::DELAY:
        {
            double task_progress_ratio = 1;
            if (t->delay_task_required != 0)
            {
                xbt_assert(t->delay_task_start != -1, "Internal error");
                double runtime = simgrid::s4u::Engine::get_clock() - t->delay_task_start;
                task_progress_ratio = runtime / t->delay_task_required;
            }

            kp->add_atomic(t->unique_name(), t->profile->name, task_progress_ratio);
        } break;
        case ProfileType::SMPI: {
            kp->add_atomic(t->unique_name(), t->profile->name, -1);
        } break;
        case ProfileType::SEQUENTIAL_COMPOSITION: {
            xbt_assert(t->sub_tasks.size() == 1, "Internal error");
            auto sub_task = t->sub_tasks[0];
            tasks.push(sub_task);
            xbt_assert(sub_task != nullptr, "Internal error");

            kp->add_sequential(t->unique_name(), t->profile->name, t->current_repetition, t->current_task_index, sub_task->unique_name());
        } break;
        default:
            xbt_die("Unimplemented kill progress of profile type %d", (int)task->profile->type);
        }
    }

    return kp;
}

/**
 * @brief Create a batprotocol::Job from a Batsim Job
 * @return The corresponding batprotocol::Job
 */
std::shared_ptr<batprotocol::Job> to_job(const Job & job)
{
    auto proto_job = batprotocol::Job::make();
    proto_job->set_host_number(job.requested_nb_res); // TODO: handle core/ghost requests
    proto_job->set_walltime(job.walltime);
    proto_job->set_profile(job.profile->name); // TODO: handle ghost jobs without profile
    // TODO: handle extra data
    // TODO: handle job rigidity

    return proto_job;
}

/**
 * @brief Returns a batprotocol::fb::FinalJobState corresponding to a given Batsim JobState
 * @param[in] state The Batsim JobState
 * @return A batprotocol::fb::FinalJobState corresponding to a given Batsim JobState
 */
batprotocol::fb::FinalJobState job_state_to_final_job_state(const JobState & state)
{
    using namespace batprotocol;

    switch (state)
    {
    case JobState::JOB_STATE_COMPLETED_SUCCESSFULLY:
        return fb::FinalJobState_COMPLETED_SUCCESSFULLY;
        break;
    case JobState::JOB_STATE_COMPLETED_FAILED:
        return fb::FinalJobState_COMPLETED_FAILED;
        break;
    case JobState::JOB_STATE_COMPLETED_WALLTIME_REACHED:
        return fb::FinalJobState_COMPLETED_WALLTIME_REACHED;
        break;
    case JobState::JOB_STATE_COMPLETED_KILLED:
        return fb::FinalJobState_COMPLETED_KILLED;
        break;
    case JobState::JOB_STATE_REJECTED:
        return fb::FinalJobState_REJECTED;
        break;
    default:
        xbt_assert(false, "Invalid (non-final) job state received: %d", static_cast<int>(state));
    }
}

batprotocol::SimulationBegins to_simulation_begins(const BatsimContext * context)
{
    using namespace batprotocol;

    SimulationBegins begins;

    // Hosts
    begins.set_host_number(context->machines.nb_machines());
    for (const Machine * machine : context->machines.compute_machines())
    {
        auto host = machine->host;
        begins.add_host(machine->id, machine->name, host->get_pstate(), host->get_pstate_count(), fb::HostState_IDLE, host->get_core_count(), machine->pstate_speeds());

        for (auto & kv : machine->properties)
        {
            begins.set_host_property(machine->id, kv.first, kv.second);
        }

        for (auto & kv : machine->zone_properties)
        {
            begins.set_host_zone_property(machine->id, kv.first, kv.second);
        }
    }

    for (const Machine * machine : context->machines.storage_machines())
    {
        auto host = machine->host;
        begins.add_host(machine->id, machine->name, host->get_pstate(), host->get_pstate_count(), fb::HostState_IDLE, host->get_core_count(), machine->pstate_speeds());
        begins.set_host_as_storage(machine->id);

        for (auto & kv : machine->properties)
        {
            begins.set_host_property(machine->id, kv.first, kv.second);
        }

        for (auto & kv : machine->zone_properties)
        {
            begins.set_host_zone_property(machine->id, kv.first, kv.second);
        }
    }

    // Workloads
    for (const auto & kv : context->workloads.workloads())
    {
        const auto & workload_name = kv.first;
        const auto * workload = kv.second;

        begins.add_workload(workload_name, workload->file);

        // TODO: add profiles if requested by the user
    }

    // Misc.
    begins.set_batsim_execution_context(context->main_args->generate_execution_context_json());
    begins.set_batsim_arguments(std::make_shared<std::vector<std::string> >(context->main_args->raw_argv));

    return begins;
}

ExecuteJobMessage * from_execute_job(const batprotocol::fb::ExecuteJobEvent * execute_job, BatsimContext * context)
{
    auto * msg = new ExecuteJobMessage;

    // Retrieve job
    JobIdentifier job_id(execute_job->job_id()->str());
    msg->job = context->workloads.job_at(job_id);

    // Build main job's allocation
    msg->job_allocation = std::make_shared<AllocationPlacement>();
    msg->job_allocation->hosts = IntervalSet::from_string_hyphen(execute_job->allocation()->host_allocation()->str());

    using namespace batprotocol::fb;
    switch (execute_job->allocation()->executor_placement_type())
    {
    case ExecutorPlacement_NONE: {
        xbt_assert("invalid ExecuteJob received: executor placement type of job's main allocation is NONE");
    } break;
    case ExecutorPlacement_PredefinedExecutorPlacementStrategyWrapper: {
        msg->job_allocation->use_predefined_strategy = true;
        msg->job_allocation->predefined_strategy = execute_job->allocation()->executor_placement_as_PredefinedExecutorPlacementStrategyWrapper()->strategy();
    } break;
    case ExecutorPlacement_CustomExecutorToHostMapping: {
        msg->job_allocation->use_predefined_strategy = false;
        auto custom_mapping = execute_job->allocation()->executor_placement_as_CustomExecutorToHostMapping()->mapping();
        msg->job_allocation->custom_mapping.reserve(custom_mapping->size());
        for (unsigned int i = 0; i < custom_mapping->size(); ++i)
        {
            msg->job_allocation->custom_mapping[i] = custom_mapping->Get(i);
        }
    } break;
    }

    // Build override allocations for profiles
    for (unsigned int i = 0; i < execute_job->profile_allocation_override()->size(); ++i)
    {
        auto override_alloc = std::make_shared<::AllocationPlacement>();
        auto override = execute_job->profile_allocation_override()->Get(i);
        auto profile_name = override->profile_id()->str();
        msg->profile_allocation_override[profile_name] = override_alloc;

        override_alloc->hosts = IntervalSet::from_string_hyphen(override->host_allocation()->str());

        switch (override->executor_placement_type())
        {
        case ExecutorPlacement_NONE: {
            xbt_assert("invalid ExecuteJob received: executor placement type of job's main allocation is NONE");
        } break;
        case ExecutorPlacement_PredefinedExecutorPlacementStrategyWrapper: {
            override_alloc->use_predefined_strategy = true;
            override_alloc->predefined_strategy = execute_job->allocation()->executor_placement_as_PredefinedExecutorPlacementStrategyWrapper()->strategy();
        } break;
        case ExecutorPlacement_CustomExecutorToHostMapping: {
            override_alloc->use_predefined_strategy = false;
            auto custom_mapping = execute_job->allocation()->executor_placement_as_CustomExecutorToHostMapping()->mapping();
            override_alloc->custom_mapping.reserve(custom_mapping->size());
            for (unsigned int i = 0; i < custom_mapping->size(); ++i)
            {
                override_alloc->custom_mapping[i] = custom_mapping->Get(i);
            }
        } break;
        }
    }

    // Storage overrides
    for (unsigned int i = 0; i < execute_job->storage_placement()->size(); ++i)
    {
        auto storage_placement = execute_job->storage_placement()->Get(i);
        msg->storage_mapping[storage_placement->storage_name()->str()] = storage_placement->host_id();
    }

    return msg;
}

RejectJobMessage *from_reject_job(const batprotocol::fb::RejectJobEvent * reject_job, BatsimContext * context)
{
    auto msg = new RejectJobMessage;

    JobIdentifier job_id(reject_job->job_id()->str());
    msg->job = context->workloads.job_at(job_id);

    return msg;
}

KillJobsMessage *from_kill_jobs(const batprotocol::fb::KillJobsEvent * kill_jobs, BatsimContext * context)
{
    auto msg = new KillJobsMessage;

    msg->job_ids.reserve(kill_jobs->job_ids()->size());
    msg->jobs.reserve(kill_jobs->job_ids()->size());
    for (unsigned int i = 0; i < kill_jobs->job_ids()->size(); ++i)
    {
        JobIdentifier job_id(kill_jobs->job_ids()->Get(i)->str());
        msg->job_ids.push_back(job_id.to_string());
        msg->jobs.push_back(context->workloads.job_at(job_id));
    }

    return msg;
}

ExternalDecisionComponentHelloMessage *from_edc_hello(const batprotocol::fb::ExternalDecisionComponentHelloEvent * edc_hello, BatsimContext * context)
{
    auto msg = new ExternalDecisionComponentHelloMessage;

    msg->batprotocol_version = edc_hello->batprotocol_version()->str();
    msg->edc_name = edc_hello->decision_component_name()->str();
    msg->edc_version = edc_hello->decision_component_version()->str();
    msg->edc_commit = edc_hello->decision_component_commit()->str();

    return msg;
}

void parse_batprotocol_message(const std::string & buffer, double & now, std::vector<IPMessageWithTimestamp> & messages, BatsimContext * context)
{
    auto parsed = flatbuffers::GetRoot<batprotocol::fb::Message>(buffer.data());
    now = parsed->now();
    messages.resize(parsed->events()->size());

    double preceding_event_timestamp = -1;
    if (parsed->events()->size() > 0)
        preceding_event_timestamp = parsed->events()->Get(0)->timestamp();

    for (unsigned int i = 0; i < parsed->events()->size(); ++i)
    {
        auto event_timestamp = parsed->events()->Get(i);
        auto ip_message = new IPMessage;

        messages[i].timestamp = event_timestamp->timestamp();
        messages[i].message = ip_message;

        xbt_assert(messages[i].timestamp <= now,
            "invalid event %u (type='%s') in message: event timestamp (%g) is after message's now (%g)",
            i, batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()], messages[i].timestamp, now
        );
        xbt_assert(messages[i].timestamp >= preceding_event_timestamp,
            "invalid event %u (type='%s') in message: event timestamp (%g) is before preceding event's timestamp (%g) while events should be in chronological order",
            i, batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()], messages[i].timestamp, preceding_event_timestamp
        );

        using namespace batprotocol::fb;
        switch (event_timestamp->event_type())
        {
        case Event_RejectJobEvent: {
            ip_message->type = IPMessageType::SCHED_REJECT_JOB;
            ip_message->data = static_cast<void *>(from_reject_job(event_timestamp->event_as_RejectJobEvent(), context));
        } break;
        case Event_ExecuteJobEvent: {
            ip_message->type = IPMessageType::SCHED_EXECUTE_JOB;
            ip_message->data = static_cast<void *>(from_execute_job(event_timestamp->event_as_ExecuteJobEvent(), context));
        } break;
        case Event_KillJobsEvent: {
            ip_message->type = IPMessageType::SCHED_KILL_JOBS;
            ip_message->data = static_cast<void *>(from_kill_jobs(event_timestamp->event_as_KillJobsEvent(), context));
        } break;
        case Event_ExternalDecisionComponentHelloEvent: {
            ip_message->type = IPMessageType::SCHED_HELLO;
            ip_message->data = static_cast<void *>(from_edc_hello(event_timestamp->event_as_ExternalDecisionComponentHelloEvent(), context));
        } break;
        default: {
            xbt_assert("Unhandled event type received (%s)", batprotocol::fb::EnumNamesEvent()[event_timestamp->event_type()]);
        } break;
        }
    }
}

} // end of namespace protocol

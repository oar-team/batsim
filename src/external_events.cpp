/**
 * @file external_events.cpp
 * @brief Contains event-related structures
 */

#include "external_events.hpp"

#include <fstream>

#include <simgrid/s4u.hpp>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(external_events, "external_events"); //!< Logging

using namespace std;
using namespace rapidjson;

std::string event_type_to_string(const ExternalEventType & type)
{
    string event_type("unknown");
    switch(type)
    {
    case ExternalEventType::GENERIC:
        event_type = "generic";
        break;
    }
    return event_type;
}

ExternalEvent::~ExternalEvent()
{
    if (data != nullptr)
    {
        switch(type)
        {
        case ExternalEventType::GENERIC:
            delete static_cast<GenericEventData *>(data);
            break;
        }

        data = nullptr;
    }
}

ExternalEventType event_type_from_string(const std::string & type_str)
{
    ExternalEventType type;
    if(type_str == "generic")
    {
        type = ExternalEventType::GENERIC;
    }
    else
    {
        xbt_assert(false, "Unknown event type: %s", type_str.c_str());
    }
    return type;
}


// Event-related functions
ExternalEvent * ExternalEvent::from_json(rapidjson::Value & json_desc,
                                         const std::string & error_prefix)
{
    xbt_assert(json_desc.IsObject(), "%s: one event is not an object", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("type"), "%s: one event has no 'type' field", error_prefix.c_str());
    xbt_assert(json_desc["type"].IsString(), "%s: one event type field is not valid, it should be a string.", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("timestamp"), "%s: one event has no 'timestamp' field", error_prefix.c_str());
    xbt_assert(json_desc["timestamp"].IsNumber(), "%s: one event timestamp field is not valid, it should be a number.", error_prefix.c_str());

    ExternalEvent * event = new ExternalEvent;

    event->type = event_type_from_string(json_desc["type"].GetString());

    event->timestamp = static_cast<long double>(json_desc["timestamp"].GetDouble());
    xbt_assert(event->timestamp >= 0, "%s: one event has a non-positive timestamp.", error_prefix.c_str());

    if (event->type == ExternalEventType::GENERIC)
    {
        //TODO: Handle unknown external event as Generic by default
        GenericEventData * data = new GenericEventData;
        data->json_desc_str = json_desc["data"].GetString();
        // rapidjson::StringBuffer buffer;
        // rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        // json_desc.Accept(writer);

        // data->json_desc_str = std::string(buffer.GetString(), buffer.GetSize());
        event->data = static_cast<void*>(data);
    }
    else
    {
        xbt_die("%s: one event has an unknown event type.", error_prefix.c_str());
    }

    return event;
}

ExternalEvent * ExternalEvent::from_json(const std::string & json_str,
                                         const std::string & error_prefix)
{
    Document doc;
    doc.Parse(json_str.c_str());
    xbt_assert(!doc.HasParseError(),
               "%s: Cannot be parsed, Content (between '##'):\n#%s#",
               error_prefix.c_str(), json_str.c_str());
    return ExternalEvent::from_json(doc, error_prefix);
}

bool event_comparator_timestamp_number(const ExternalEvent * a, const ExternalEvent * b)
{
    if (a->timestamp == b->timestamp)
    {
        return a->type < b->type;
    }
    return a->timestamp < b->timestamp;
}


// Events-related functions
ExternalEventList::ExternalEventList(const string &name, const bool is_static) :
    _name(name),
    _is_static(is_static)
{
}

ExternalEventList::~ExternalEventList()
{
    for(auto & event: _events)
    {
        delete event;
    }
}

void ExternalEventList::load_from_json(const std::string & json_filename)
{
    XBT_INFO("Loading JSON events from '%s' ...", json_filename.c_str());
    _file = json_filename;

    std::ifstream ifile(json_filename.c_str());
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", json_filename.c_str());
    string line;
    while(getline(ifile, line))
    {
        if(line.size() > 0)
        {
            Document doc;
            doc.Parse(line.c_str());
            xbt_assert(!doc.HasParseError() and doc.IsObject(), "Invalid JSON event file %s, an event could not be parsed.", json_filename.c_str());

            ExternalEvent * event = ExternalEvent::from_json(doc);
            add_event(event);
        }
    }
    sort(_events.begin(), _events.end(), event_comparator_timestamp_number);

    XBT_INFO("JSON events sucessfully parsed. Read %lu events.", _events.size());
    ifile.close();
}

std::vector<ExternalEvent*> &ExternalEventList::events()
{
    return _events;
}

const std::vector<ExternalEvent*> &ExternalEventList::events() const
{
    return _events;
}

void ExternalEventList::add_event(ExternalEvent * event)
{
    _events.push_back(event);
}

bool ExternalEventList::is_static() const
{
    return _is_static;
}

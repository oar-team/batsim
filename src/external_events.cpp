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
        xbt_assert(false, "Unknown external event type: %s", type_str.c_str());
    }
    return type;
}


// Event-related functions
ExternalEvent * ExternalEvent::from_json(rapidjson::Value & json_desc,
                                         const std::string & error_prefix)
{
    xbt_assert(json_desc.IsObject(), "%s: one external event is not an object", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("type"), "%s: one external event has no 'type' field", error_prefix.c_str());
    xbt_assert(json_desc["type"].IsString(), "%s: one external event type field is not valid, it should be a string.", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("timestamp"), "%s: one external event has no 'timestamp' field", error_prefix.c_str());
    xbt_assert(json_desc["timestamp"].IsNumber(), "%s: one external event timestamp field is not valid, it should be a number.", error_prefix.c_str());

    ExternalEvent * event = new ExternalEvent;

    event->type = event_type_from_string(json_desc["type"].GetString());

    event->timestamp = static_cast<long double>(json_desc["timestamp"].GetDouble());
    xbt_assert(event->timestamp >= 0, "%s: one external event has a non-positive timestamp.", error_prefix.c_str());

    if (event->type == ExternalEventType::GENERIC)
    {
        //TODO: Handle unknown external event as Generic by default
        GenericEventData * data = new GenericEventData;
        data->json_desc_str = json_desc["data"].GetString();
        event->data = static_cast<void*>(data);
    }
    else
    {
        xbt_die("%s: one external event has an unknown type.", error_prefix.c_str());
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

void ExternalEvent::set_id(const std::string & id)
{
    this->id = id;
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
    name(name),
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
    XBT_INFO("Loading JSON external events from '%s' ...", json_filename.c_str());
    filename = json_filename;

    int next_id = 0;

    std::ifstream ifile(json_filename.c_str());
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", json_filename.c_str());
    string line;
    while(getline(ifile, line))
    {
        if(line.size() > 0)
        {
            Document doc;
            doc.Parse(line.c_str());
            xbt_assert(!doc.HasParseError() and doc.IsObject(), "Invalid JSON external events file %s, an external event could not be parsed.", json_filename.c_str());

            ExternalEvent * event = ExternalEvent::from_json(doc);
            event->set_id(name+"!"+std::to_string(next_id));
            add_event(event);
            next_id++;
        }
    }
    sort(_events.begin(), _events.end(), event_comparator_timestamp_number);

    XBT_INFO("JSON external events sucessfully parsed. Read %lu external events.", _events.size());
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

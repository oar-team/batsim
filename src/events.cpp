/**
 * @file events.cpp
 * @brief Contains event-related structures
 */

#include "events.hpp"

#include <fstream>

#include <simgrid/s4u.hpp>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(events, "events"); //!< Logging

using namespace std;
using namespace rapidjson;

std::string event_type_to_string(const EventType & type)
{
    string event_type("unknown");
    switch(type)
    {
    case EventType::EVENT_MACHINE_AVAILABLE:
        event_type = "machine_available";
        break;
    case EventType::EVENT_MACHINE_UNAVAILABLE:
        event_type = "machine_unavailable";
        break;
    case EventType::EVENT_GENERIC:
        event_type = "generic";
        break;
    }
    return event_type;
}

Event::~Event()
{
    if (data != nullptr)
    {
        switch(type)
        {
        case EventType::EVENT_MACHINE_AVAILABLE:
        case EventType::EVENT_MACHINE_UNAVAILABLE: // consecutive cases without break/etc. is intended here
            delete static_cast<MachineAvailabilityEventData *>(data);
            break;
        case EventType::EVENT_GENERIC:
            delete static_cast<GenericEventData *>(data);
            break;
        }

        data = nullptr;
    }
}

EventType event_type_from_string(const std::string & type_str, const bool unknown_as_generic)
{
    EventType type;
    if(type_str == "machine_available")
    {
        type = EventType::EVENT_MACHINE_AVAILABLE;
    }
    else if (type_str == "machine_unavailable")
    {
        type = EventType::EVENT_MACHINE_UNAVAILABLE;
    }
    else
    {
        if (unknown_as_generic)
        {
            type = EventType::EVENT_GENERIC;
        }
        else
        {
            xbt_assert(false, "Unknown event type: %s", type_str.c_str());
        }
    }
    return type;
}


// Event-related functions
Event * Event::from_json(rapidjson::Value & json_desc,
                         const bool unknown_as_generic,
                         const std::string & error_prefix)
{
    xbt_assert(json_desc.IsObject(), "%s: one event is not an object", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("type"), "%s: one event has no 'type' field", error_prefix.c_str());
    xbt_assert(json_desc["type"].IsString(), "%s: one event type field is not valid, it should be a string.", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("timestamp"), "%s: one event has no 'timestamp' field", error_prefix.c_str());
    xbt_assert(json_desc["timestamp"].IsNumber(), "%s: one event timestamp field is not valid, it should be a number.", error_prefix.c_str());

    Event * event = new Event;

    event->type = event_type_from_string(json_desc["type"].GetString(), unknown_as_generic);

    event->timestamp = static_cast<long double>(json_desc["timestamp"].GetDouble());
    xbt_assert(event->timestamp >= 0, "%s: one event has a non-positive timestamp.", error_prefix.c_str());

    if (event->type == EventType::EVENT_MACHINE_AVAILABLE ||
        event->type == EventType::EVENT_MACHINE_UNAVAILABLE)
    {
        xbt_assert(json_desc.HasMember("resources"), "%s: one event has no 'resources' field", error_prefix.c_str());
        xbt_assert(json_desc["resources"].IsString(), "%s: one event resources field is not valid, it should be a string.", error_prefix.c_str());

        MachineAvailabilityEventData * data = new MachineAvailabilityEventData;

        try { data->machine_ids = IntervalSet::from_string_hyphen(json_desc["resources"].GetString(), " ", "-"); }
        catch(const std::exception & e) { throw std::runtime_error(std::string("Invalid JSON message: ") + e.what());}

        event->data = static_cast<void*>(data);
    }
    else if ((event->type == EventType::EVENT_GENERIC) && unknown_as_generic)
    {
        GenericEventData * data = new GenericEventData;
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_desc.Accept(writer);

        data->json_desc_str = std::string(buffer.GetString(), buffer.GetSize());
        event->data = static_cast<void*>(data);
    }
    else
    {
        xbt_die("%s: one event has an unknown event type.", error_prefix.c_str());
    }

    return event;
}

Event * Event::from_json(const std::string & json_str,
                         const bool unknown_as_generic,
                         const std::string & error_prefix)
{
    Document doc;
    doc.Parse(json_str.c_str());
    xbt_assert(!doc.HasParseError(),
               "%s: Cannot be parsed, Content (between '##'):\n#%s#",
               error_prefix.c_str(), json_str.c_str());
    return Event::from_json(doc, unknown_as_generic, error_prefix);
}

bool event_comparator_timestamp_number(const Event * a, const Event * b)
{
    if (a->timestamp == b->timestamp)
    {
        return a->type < b->type;
    }
    return a->timestamp < b->timestamp;
}


// Events-related functions
EventList::EventList(const string &name, const bool is_static) :
    _name(name),
    _is_static(is_static)
{
}

EventList::~EventList()
{
    for(auto & event: _events)
    {
        delete event;
    }
}

void EventList::load_from_json(const std::string & json_filename, bool unknown_as_generic)
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

            Event * event = Event::from_json(doc, unknown_as_generic);
            add_event(event);
        }
    }
    sort(_events.begin(), _events.end(), event_comparator_timestamp_number);

    XBT_INFO("JSON events sucessfully parsed. Read %lu events.", _events.size());
    ifile.close();
}

std::vector<Event*> &EventList::events()
{
    return _events;
}

const std::vector<Event*> &EventList::events() const
{
    return _events;
}

void EventList::add_event(Event * event)
{
    _events.push_back(event);
}

bool EventList::is_static() const
{
    return _is_static;
}

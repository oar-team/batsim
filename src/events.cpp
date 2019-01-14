/**
 * @file events.cpp
 * @brief Contains event-related structures
 */

#include "events.hpp"

#include <fstream>

#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(events, "events");

using namespace std;
using namespace rapidjson;

std::string event_type_to_string(const EventType & type)
{
    string event_type("UNKNOWN");
    switch(type)
    {
    case EventType::EVENT_MACHINE_FAILURE:
        event_type = "machine_failure";
        break;
    case EventType::EVENT_MACHINE_RESTORE:
        event_type = "machine_restore";
        break;
    }
    return event_type;
}

EventType event_type_from_string(const std::string & type_str)
{
    EventType type;
    if(type_str == "machine_failure")
    {
        type = EventType::EVENT_MACHINE_FAILURE;
    }
    else if (type_str == "machine_restore")
    {
        type = EventType::EVENT_MACHINE_RESTORE;
    }
    else
    {
        xbt_assert(false, "Unknown event type.");
    }
    return type;
}


// Event-related functions
Event * Event::from_json(const rapidjson::Value & json_desc,
                         const std::string & error_prefix)
{
    xbt_assert(json_desc.IsObject(), "%s: one event is not an object", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("type"), "%s: one event has no 'type' field", error_prefix.c_str());
    xbt_assert(json_desc["type"].IsString(), "%s: one event type field is not valid, it should be a string.", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("resources"), "%s: one event has no 'resources' field", error_prefix.c_str());
    xbt_assert(json_desc["resources"].IsString(), "%s: one event resources field is not valid, it should be a string.", error_prefix.c_str());

    xbt_assert(json_desc.HasMember("timestamp"), "%s: one event has no 'timestamp' field", error_prefix.c_str());
    xbt_assert(json_desc["timestamp"].IsNumber(), "%s: one event timestamp field is not valid, it should be a number.", error_prefix.c_str());

    Event * event = new Event;

    event->type = event_type_from_string(json_desc["type"].GetString());

    event->timestamp = json_desc["timestamp"].GetDouble();
    xbt_assert(event->timestamp >= 0, "%s: one event has a non-positive timestamp.", error_prefix.c_str());

    try { event->machine_ids = IntervalSet::from_string_hyphen(json_desc["resources"].GetString(), " ", "-"); }
    catch(const std::exception & e) { throw std::runtime_error(std::string("Invalid JSON message: ") + e.what());}

    return event;
}

Event * Event::from_json(const std::string & json_str,
                         const std::string & error_prefix)
{
    Document doc;
    doc.Parse(json_str.c_str());
    xbt_assert(!doc.HasParseError(),
               "%s: Cannot be parsed, Content (between '##'):\n#%s#",
               error_prefix.c_str(), json_str.c_str());
    return Event::from_json(doc, error_prefix);
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
EventList * EventList::new_eventList_from_json(const std::string & name,
                                      const std::string & filename)
{
    XBT_INFO("Loading JSON events '%s' ...", filename.c_str());
    EventList * ev = new EventList;
    ev->_name = name;
    ev->_file = filename;

    std::ifstream ifile(filename.c_str());
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", filename.c_str());
    string line;
    while(getline(ifile, line))
    {
        if(line.size() > 0)
        {
            Document doc;
            doc.Parse(line.c_str());
            xbt_assert(!doc.HasParseError() and doc.IsObject(), "Invalid JSON event file %s, an event could not be parsed.", filename.c_str());

            Event * event = Event::from_json(doc);
            ev->add_event(event);
        }
    }
    sort(ev->_events.begin(), ev->_events.end(), event_comparator_timestamp_number);

    XBT_INFO("JSON events sucessfully parsed. Read %lu events.", ev->_events.size());
    ifile.close();
    return ev;
}

EventList * EventList::new_dynamic_eventList(const std::string & name)
{
    EventList * ev = new EventList;
    ev->_name = name;
    ev->_is_static = false;
    return ev;
}

std::vector<Event*> &EventList::getEvents()
{
    return _events;
}

const std::vector<Event*> &EventList::getEvents() const
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

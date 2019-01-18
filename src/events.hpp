/**
 * @file events.hpp
 * @brief Contains events-related classes
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <intervalset.hpp>

/**
 * @brief Contains the different types of event
 */
enum class EventType
{
     EVENT_MACHINE_FAILURE      //!< The machine becomes unavailable (failure)
    ,EVENT_MACHINE_RESTORE      //!< The machine becomes available (restore)
};

std::string event_type_to_string(const EventType & type);
EventType event_type_from_string(const std::string & type_str);

/**
 * @brief Represents an event
 */
struct Event
{
    Event() = default;
    ~Event();

    EventType type;
    long double timestamp;
    IntervalSet resources;

public:
    /**
     * @brief Creates a new-allocated Event from a JSON description
     * @param[in] json_desc The JSON description of the event
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The newly allocated Event
     */
    static Event * from_json(const rapidjson::Value & json_desc,
                             const std::string & error_prefix = "Invalid JSON event");

    /**
     * @brief Creates a new-allocated Event from a JSON description
     * @param[in] json_desc The JSON description of the event
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The newly allocated Event
     */
    static Event * from_json(const std::string & json_str,
                             const std::string & error_prefix = "Invalid JSON event");

};

/**
 * @brief Compares events thanks to their timestamps
 * @param a The first Event
 * @param b The second Event
 * @return  True if and only if the first event's timestamp is strictly lower than the second event's timestamp
 */
bool event_comparator_timestamp_number(const Event * a, const Event * b);


struct Events
{
public:
    Events() = default;
    ~Events();

    static Events * new_events_from_json(const std::string & filename,
                                         const std::string & name);

    static Events * new_dynamic_events(const std::string & name);

    std::vector<Event*> & getEvents();

    const std::vector<Event*> & getEvents() const;

    void add_event(Event * event);

    bool is_static() const;

private:
    std::vector<Event*> _events;    //!< The list of events (should be sorted in non-decreasing timestamp)
    std::string _name;              //!< The name of the event list
    std::string _file = "";         //!< The filename of the event list, if any
    bool _is_static = true;
};

typedef std::map<std::string, Events*> EventsMap;

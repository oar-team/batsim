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

/**
 * @brief Returns a std::string corresponding to a given EventType
 * @param[in] type The Event type
 * @return A std::string corresponding to a given EventType
 */
std::string event_type_to_string(const EventType & type);

/**
 * @brief Returns an EventType corresponding to a given std::string
 * @param type_str The std::string
 * @return An EventType corresponding to a given std::string
 */
EventType event_type_from_string(const std::string & type_str);

/**
 * @brief Represents an event
 */
struct Event
{
    Event() = default;
    ~Event();

    EventType type;             //!< The type of the Event
    long double timestamp;      //!< The occurring time of the Event
    IntervalSet machine_ids;    //!< The machine ids involved in the Event

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
     * @param[in] json_str The JSON description of the event (as a string)
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

/**
 * @brief List of Events to be submitted via an event_submitter
 */
struct EventList
{
public:
    EventList() = default;
    ~EventList();

    /**
     * @brief Builds an EventList from a JSON filename
     * @param[in] filename The name of the JSON file
     * @param[in] name The name of the EventList
     * @return The created EventList
     */
    static EventList * new_eventList_from_json(const std::string & filename,
                                         const std::string & name);

    /**
     * @brief Builds an empty dynamic EventList
     * @details Dynamic Events are created by the decision process
     * @param[in] name The EventList name
     * @return The created EventList
     */
    static EventList * new_dynamic_eventList(const std::string & name);

    /**
     * @brief Gets the list of Events
     * @return The internal vector of Event*
     */
    std::vector<Event*> & getEvents();

    /**
     * @brief Gets the list of Events (const version)
     * @return The internal vector of Event* (const version)
     */
    const std::vector<Event*> & getEvents() const;

    /**
     * @brief Adds an event to the EventList
     * @param[in] event The Event to add to the list
     */
    void add_event(Event * event);

    /**
     * @brief Returns whether the EventList is static (corresponding to a Batsim input EventList) or not
     * @return  Whether the EventList is static or not
     */
    bool is_static() const;

private:
    std::vector<Event*> _events;    //!< The list of events (should be sorted in non-decreasing timestamp)
    std::string _name;              //!< The name of the event list
    std::string _file = "";         //!< The filename of the event list, if any
    bool _is_static = true;         //!< Whether the EventList is dynamic
};

typedef std::map<std::string, EventList*> EventListMap; //!< Associates EventLists with their names

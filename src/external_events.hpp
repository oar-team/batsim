/**
 * @file external_events.hpp
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
enum class ExternalEventType
{
    GENERIC                  //!< A generic event
};

/**
 * @brief Returns a std::string corresponding to a given EventType
 * @param[in] type The ExternalEvent type
 * @return A std::string corresponding to a given EventType
 */
std::string event_type_to_string(const ExternalEventType & type);

/**
 * @brief Returns an ExternalEventType corresponding to a given std::string
 * @param type_str The std::string
 * @return An EventType corresponding to a given std::string
 */
ExternalEventType event_type_from_string(const std::string & type_str);


/**
 * @brief The data of an EVENT_GENERIC
 */
struct GenericEventData
{
    std::string json_desc_str; //!< The json description of the generic event as a string
};

/**
 * @brief Represents an event
 */
struct ExternalEvent
{
    ExternalEvent() = default;
    ~ExternalEvent();

    ExternalEventType type;             //!< The type of the ExternalEvent
    long double timestamp = -1; //!< The occurring time of the ExternalEvent
    void * data = nullptr;      //!< The ExternalEvent data

public:
    /**
     * @brief Creates a new-allocated Event from a JSON description
     * @param[in] json_desc The JSON description of the event
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The newly allocated ExternalEvent
     */
    static ExternalEvent * from_json(rapidjson::Value & json_desc,
                             const std::string & error_prefix = "Invalid JSON event");

    /**
     * @brief Creates a new-allocated Event from a JSON description
     * @param[in] json_str The JSON description of the event (as a string)
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The newly allocated Externalvent
     */
    static ExternalEvent * from_json(const std::string & json_str,
                             const std::string & error_prefix = "Invalid JSON event");

};

/**
 * @brief Compares events thanks to their timestamps
 * @param a The first ExternalEvent
 * @param b The second ExternalEvent
 * @return  True if and only if the first event's timestamp is strictly lower than the second event's timestamp
 */
bool event_comparator_timestamp_number(const ExternalEvent * a, const ExternalEvent * b);

/**
 * @brief List of Events to be submitted via an event_submitter
 */
class ExternalEventList
{
public:
    /**
     * @brief Creates an empty ExternalEventList
     * @param[in] name The name of the ExternalEventList
     * @param[in] is_static Whether this ExternalEventList is static or not
     */
    explicit ExternalEventList(const std::string & name = "unset", const bool is_static = true);

    /**
      * @brief Destroys an ExternalEventList
      */
    ~ExternalEventList();

    /**
     * @brief Loads static ExternalEvents from a JSON filename
     * @param[in] json_filename The name of the JSON file
     */
    void load_from_json(const std::string & json_filename);

    /**
     * @brief Gets the list of ExternalEvents
     * @return The internal vector of ExternalEvent*
     */
    std::vector<ExternalEvent*> & events();

    /**
     * @brief Gets the list of Events (const version)
     * @return The internal vector of ExternalEvent* (const version)
     */
    const std::vector<ExternalEvent*> & events() const;

    /**
     * @brief Adds an event to the ExternalEventList
     * @param[in] event The ExternalEvent to add to the list
     */
    void add_event(ExternalEvent * event);

    /**
     * @brief Returns whether the ExternalEventList is static (corresponding to a Batsim input ExternalEventList) or not
     * @return  Whether the ExternalEventList is static or not
     */
    bool is_static() const;

private:
    std::vector<ExternalEvent*> _events;    //!< The list of events (should be sorted in non-decreasing timestamp)
    std::string _name;              //!< The name of the event list
    std::string _file = "";         //!< The filename of the event list, if any
    bool _is_static;                //!< Whether the EventList is dynamic
};

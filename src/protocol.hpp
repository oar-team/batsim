#pragma once

#include <functional>
#include <vector>
#include <string>
#include <map>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <intervalset.hpp>

#include "machines.hpp"
#include "workload.hpp"
#include "ipp.hpp"

struct BatsimContext;

/**
 * @brief Custom rapidjson Writer to force fixed float writing precision
 */
template<typename OutputStream>
class Writer : public rapidjson::Writer<OutputStream>
{
public:
    /**
     * @brief Constructor
     * @param[in,out] os The output stream
     */
    explicit Writer(OutputStream& os) : rapidjson::Writer<OutputStream>(os), os_(&os)
    {
    }

    /**
     * @brief Adds a double in the output stream
     * @param[in] d The double to add in the stream
     * @return true on success, false otherwise
     */
    bool Double(double d)
    {
        this->Prefix(rapidjson::kNumberType);

        const int buf_size = 32;
        char * buffer = new char[buf_size];

        int ret = snprintf(buffer, buf_size, "%6f", d);
        RAPIDJSON_ASSERT(ret >= 1);
        RAPIDJSON_ASSERT(ret < buf_size - 1);

        for (int i = 0; i < ret; ++i)
        {
            os_->Put(buffer[i]);
        }

        delete[] buffer;
        return ret < (buf_size - 1);
    }

private:
    OutputStream* os_; //!< The output stream
};

/**
 * @brief Does the interface between protocol semantics and message representation.
 */
class AbstractProtocolWriter
{
public:
    /**
     * @brief Destructor
     */
    virtual ~AbstractProtocolWriter() {}

    // Messages from Batsim to the Scheduler
    /**
     * @brief Appends a SIMULATION_BEGINS event.
     * @param[in] machines The machines usable to compute jobs
     * @param[in] workloads The workloads given to batsim
     * @param[in] configuration The simulation configuration
     * @param[in] allow_compute_sharing Whether sharing is enabled on compute machines
     * @param[in] allow_storage_sharing Whether sharing is enabled on storage machines
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_simulation_begins(Machines & machines,
                                          Workloads & workloads,
                                          const rapidjson::Document & configuration,
                                          bool allow_compute_sharing,
                                          bool allow_storage_sharing,
                                          double date) = 0;

    /**
     * @brief Appends a SIMULATION_ENDS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_simulation_ends(double date) = 0;

    /**
     * @brief Appends a JOB_SUBMITTED event.
     * @param[in] job_id The identifier of the submitted job.
     * @param[in] job_json_description The job JSON description (optional if redis is enabled)
     * @param[in] profile_json_description The profile JSON description (optional if redis is
     *            disabled or if profiles are not forwarded)
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_job_submitted(const std::string & job_id,
                                      const std::string & job_json_description,
                                      const std::string & profile_json_description,
                                      double date) = 0;

    /**
     * @brief Appends a JOB_COMPLETED event.
     * @param[in] job_id The identifier of the job that has completed.
     * @param[in] job_state The job state
     * @param[in] job_alloc last allocation of the job
     * @param[in] return_code The job return code
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_job_completed(const std::string & job_id,
                                      const std::string & job_state,
                                      const std::string & job_alloc,
                                      int return_code,
                                      double date) = 0;

    /**
     * @brief Appends a JOB_KILLED event.
     * @param[in] job_ids The identifiers of the jobs that have been killed.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     * @param[in] job_progress Contains the progress of each job that has really been killed.
     */
    virtual void append_job_killed(const std::vector<std::string> & job_ids,
                                   const std::map<std::string, BatTask *> & job_progress,
                                   double date) = 0;

    /**
     * @brief Appends a FROM_JOB_MSG event.
     * @param[in] job_id The identifier of the job which sends the message.
     * @param[in] message The message to be sent to the scheduler.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_from_job_message(const std::string & job_id,
                                         const rapidjson::Document & message,
                                         double date) = 0;

    /**
     * @brief Appends a RESOURCE_STATE_CHANGED event.
     * @param[in] resources The resources whose state has changed.
     * @param[in] new_state The state the machines are now in.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_resource_state_changed(const IntervalSet & resources,
                                               const std::string & new_state,
                                               double date) = 0;

    /**
     * @brief Appends a QUERY message to ask the scheduler about the waiting time of a potential job.
     * @param[in] job_id The identifier of the potential job
     * @param[in] job_json_description The job JSON description of the potential job
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_query_estimate_waiting_time(const std::string & job_id,
                                                    const std::string & job_json_description,
                                                    double date) = 0;

    /**
     * @brief Appends an ANSWER (energy) event.
     * @param[in] consumed_energy The total consumed energy in joules
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_answer_energy(double consumed_energy,
                                      double date) = 0;

    /**
     * @brief Appends a NOTIFY event.
     * @param notify_type The type of the NOTIFY event
     * @param date The event date. Must be greater than or equal to the previous event date.
     */
    virtual void append_notify(const std::string & notify_type,
                               double date) = 0;

    /**
     * @brief Appends a NOTIFY event related to resource events.
     * @param notify_type The type of the resource event
     * @param resources The list of resources involved by the event
     * @param date The event date. Must be greater than or equal to the previous event date.
     */
    virtual void append_notify_resource_event(const std::string & notify_type,
                                              const IntervalSet & resources,
                                              double date) = 0;

    /**
     * @brief Appends a NOTIFY event related to a generic external event.
     * @param[in] json_desc The JSON description of the generic event
     * @param[in] date The event date. Must be greater than or equal to the previous event date.
     */
    virtual void append_notify_generic_event(const std::string & json_desc,
                                             double date) = 0;

    /**
     * @brief Appends a REQUESTED_CALL message.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_requested_call(double date) = 0;

    // Management functions
    /**
     * @brief Clears inner content. Should called directly after generate_current_message.
     */
    virtual void clear() = 0;

    /**
     * @brief Generates a string representation of the message containing all the events since the
     *        last call to clear.
     * @param[in] date The message date. Must be greater than or equal to the inner events dates.
     * @return A string representation of the events added since the last call to clear.
     */
    virtual std::string generate_current_message(double date) = 0;

    /**
     * @brief Returns whether the Writer has content
     * @return Whether the Writer has content
     */
    virtual bool is_empty() = 0;
};

/**
 * @brief The JSON implementation of the AbstractProtocolWriter
 */
class JsonProtocolWriter : public AbstractProtocolWriter
{
public:
    /**
     * @brief Creates an empty JsonProtocolWriter
     * @param[in,out] context The BatsimContext
     */
    explicit JsonProtocolWriter(BatsimContext * context);

    /**
     * @brief JsonProtocolWriter cannot be copied.
     * @param[in] other Another instance
     */
    JsonProtocolWriter(const JsonProtocolWriter & other) = delete;

    /**
     * @brief Destroys a JsonProtocolWriter
     */
    ~JsonProtocolWriter();

    // Messages from Batsim to the Scheduler
    /**
     * @brief Appends a SIMULATION_BEGINS event.
     * @param[in] machines The machines usable to compute jobs
     * @param[in] workloads The workloads given to batsim
     * @param[in] configuration The simulation configuration
     * @param[in] allow_compute_sharing Whether sharing is enabled on compute machines
     * @param[in] allow_storage_sharing Whether sharing is enabled on storage machines
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_simulation_begins(Machines & machines,
                                  Workloads & workloads,
                                  const rapidjson::Document & configuration,
                                  bool allow_compute_sharing,
                                  bool allow_storage_sharing,
                                  double date);

    /**
     * @brief Appends a SIMULATION_ENDS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_simulation_ends(double date);

    /**
     * @brief Appends a JOB_SUBMITTED event.
     * @param[in] job_id The identifier of the submitted job.
     * @param[in] job_json_description The job JSON description (optional if redis is enabled)
     * @param[in] profile_json_description The profile JSON description (optional if redis is
     *            disabled or if profiles are not forwarded)
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_job_submitted(const std::string & job_id,
                              const std::string & job_json_description,
                              const std::string & profile_json_description,
                              double date);

    /**
     * @brief Appends a JOB_COMPLETED event.
     * @param[in] job_id The identifier of the job that has completed.
     * @param[in] job_state The job state
     * @param[in] job_alloc last allocation of the job
     * @param[in] return_code The job return code
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_job_completed(const std::string & job_id,
                              const std::string & job_state,
                              const std::string & job_alloc,
                              int return_code,
                              double date);

    /**
     * @brief Appends a JOB_KILLED event.
     * @param[in] job_ids The identifiers of the jobs that have been killed.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     * @param[in] job_progress Contains the progress of each job that has really been killed.
     */
    void append_job_killed(const std::vector<std::string> & job_ids,
                           const std::map<std::string, BatTask *> & job_progress,
                           double date);

    /**
     * @brief Appends a FROM_JOB_MSG event.
     * @param[in] job_id The identifier of the job which sends the message.
     * @param[in] message The message to be sent to the scheduler.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_from_job_message(const std::string & job_id,
                                 const rapidjson::Document & message,
                                 double date);

    /**
     * @brief Appends a RESOURCE_STATE_CHANGED event.
     * @param[in] resources The resources whose state has changed.
     * @param[in] new_state The state the machines are now in.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_resource_state_changed(const IntervalSet & resources,
                                       const std::string & new_state,
                                       double date);

    /**
     * @brief Appends a QUERY message to ask the scheduler about the waiting time of a potential job.
     * @param[in] job_id The identifier of the potential job
     * @param[in] job_json_description The job JSON description of the potential job
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_query_estimate_waiting_time(const std::string & job_id,
                                            const std::string & job_json_description,
                                            double date);

    /**
     * @brief Appends an ANSWER (energy) event.
     * @param[in] consumed_energy The total consumed energy in joules
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_answer_energy(double consumed_energy,
                              double date);

    /**
     * @brief Appends a NOTIFY event
     * @param notify_type The type of the notify event
     * @param date The event date. Must be greater than or equal to the previous event.
     */
    void append_notify(const std::string & notify_type,
                       double date);

    /**
     * @brief Appends a NOTIFY event related to resource events.
     * @param notify_type The type of the resource event
     * @param resources The list of resources involved by the event
     * @param date The event date. Must be greater than or equal to the previous event date.
     */
    void append_notify_resource_event(const std::string & notify_type,
                                      const IntervalSet & resources,
                                      double date);

    /**
     * @brief Appends a NOTIFY event related to a generic external event.
     * @param[in] json_desc The JSON description of the generic event
     * @param[in] date The event date. Must be greater than or equal to the previous event date.
     */
    void append_notify_generic_event(const std::string & json_desc,
                                     double date);

    /**
     * @brief Appends a REQUESTED_CALL message.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_requested_call(double date);

    // Management functions
    /**
     * @brief Clears inner content. Should be called directly after generate_current_message.
     */
    void clear();

    /**
     * @brief Generates a string representation of the message containing all the events since the
     *        last call to clear.
     * @param[in] date The message date. Must be greater than or equal to the inner events dates.
     * @return A string representation of the events added since the last call to clear.
     */
    std::string generate_current_message(double date);

    /**
     * @brief Returns whether the Writer has content
     * @return Whether the Writer has content
     */
    bool is_empty() { return _is_empty; }

private:
    /**
     * @brief Converts a machine to a json value.
     * @param[in] machine The machine to be converted
     * @return The json value
     */
    rapidjson::Value machine_to_json_value(const Machine & machine);

private:
    BatsimContext * _context; //!< The BatsimContext
    bool _is_empty = true; //!< Stores whether events have been pushed into the writer since last clear.
    double _last_date = -1; //!< The date of the latest pushed event/message
    rapidjson::Document _doc; //!< A rapidjson document
    rapidjson::Document::AllocatorType & _alloc; //!< The allocated of _doc
    rapidjson::Value _events = rapidjson::Value(rapidjson::kArrayType); //!< A rapidjson array in which the events are pushed
};



/**
 * @brief In charge of parsing a protocol message and injecting internal messages into the simulation
 */
class AbstractProtocolReader
{
public:
    /**
     * @brief Destructor
     */
    virtual ~AbstractProtocolReader() {}

    /**
     * @brief Parses a message and injects events in the simulation
     * @param[in] message The protocol message
     */
    virtual void parse_and_apply_message(const std::string & message) = 0;
};

/**
 * @brief In charge of parsing a JSON message and injecting messages into the simulation
 */
class JsonProtocolReader : public AbstractProtocolReader
{
public:
    /**
     * @brief Constructor
     * @param[in] context The BatsimContext
     */
    explicit JsonProtocolReader(BatsimContext * context);

    /**
     * @brief JsonProtocolReader cannot be copied.
     * @param[in] other Another instance
     */
    JsonProtocolReader(const JsonProtocolReader & other) = delete;

    /**
     * @brief Destructor
     */
    ~JsonProtocolReader();

    /**
     * @brief Parses a message and injects events in the simulation
     * @param[in] message The protocol message
     */
    void parse_and_apply_message(const std::string & message);

    /**
     * @brief Parses an event and injects it in the simulation
     * @param[in] event_object The event (JSON object)
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] now The message timestamp
     */
    void parse_and_apply_event(const rapidjson::Value & event_object, int event_number, double now);


    /**
     * @brief Handles a QUERY event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_query(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles an ANSWER event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_answer(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a REJECT_JOB event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_reject_job(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles an EXECUTE_JOB event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_execute_job(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles an CHANGE_JOB_STATE event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_change_job_state(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a CALL_ME_LATER event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_call_me_later(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a SET_RESOURCE_STATE event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_set_resource_state(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a SET_JOB_METADATA event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_set_job_metadata(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a NOTIFY event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_notify(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a TO_JOB_MSG event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_to_job_msg(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a REGISTER_JOB event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_register_job(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a REGISTER_PROFILE event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_register_profile(int event_number, double timestamp, const rapidjson::Value & data_object);

    /**
     * @brief Handles a KILL_JOB event
     * @param[in] event_number The event number in [0,nb_events[.
     * @param[in] timestamp The event timestamp
     * @param[in] data_object The data associated with the event (JSON object)
     */
    void handle_kill_job(int event_number, double timestamp, const rapidjson::Value & data_object);

private:
    /**
     * @brief Sends a message at a given time, sleeping to reach the given time if needed
     * @param[in] when The date at which the message should be sent
     * @param[in] destination_mailbox The destination mailbox
     * @param[in] type The message type
     * @param[in] data The message data
     */
    void send_message_at_time(double when,
                      const std::string & destination_mailbox,
                      IPMessageType type,
                      void * data = nullptr) const;

private:
    //! Maps message types to their handler functions
    std::map<std::string, std::function<void(JsonProtocolReader*, int, double, const rapidjson::Value&)>> _type_to_handler_map;
    std::vector<std::string> accepted_requests = {"consumed_energy"}; //!< The currently acceptes requests for the QUERY_REQUEST message
    BatsimContext * context = nullptr; //!< The BatsimContext
};

#pragma once

#include <vector>
#include <string>

#include <rapidjson/document.h>

#include "machine_range.hpp"

/**
 * @brief Does the interface between protocol semantics and message representation.
 */
class AbstractProtocolWriter
{
public:
    /**
     * @brief Destructor
     */
    virtual ~AbstractProtocolWriter() {};

    // Bidirectional messages
    /**
     * @brief Appends a NOP message.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_nop(double date) = 0;

    // Messages from the Scheduler to Batsim
    /**
     * @brief Appends a SUBMIT_JOB event.
     * @details The job_description and profile_descriptions are either both given or both empty.
     *          If they are given, the job and profile information is sent within the protocol.
     *          Otherwise, it is sent by another channel (probably redis).
     * @param[in] job_id The job identifier. It must not conflict with existing job identifiers
     *            within Batism.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     * @param[in] job_description The job description string. Can be empty.
     * @param[in] profile_description The profile description string. Can be empty.
     * @param[in] acknowledge_submission Whether Batsim should acknowledge the upcoming job
     *            submission or not.
     */
    virtual void append_submit_job(const std::string & job_id,
                                   double date,
                                   const std::string & job_description = "",
                                   const std::string & profile_description = "",
                                   bool acknowledge_submission = false) = 0;

    /**
     * @brief Appends a EXECUTE_JOB event.
     * @param[in] job_id The job identifier. It must be known by Batsim.
     * @param[in] allocated_resources The resources on which the job should be executed.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     * @param[in] executor_to_allocated_resource_mapping Optional.
     *            Allows to give a custom mapping from executors to allocated resources.
     *            By default, the number of allocated resources must equals
     *            the job size, and executor i is launched on allocated resource i.
     */
    virtual void append_execute_job(const std::string & job_id,
                                    const MachineRange & allocated_resources,
                                    double date,
                                    const std::string & executor_to_allocated_resource_mapping = "") = 0;

    /**
     * @brief Appends a REJECT_JOB event.
     * @param[in] job_id The job identifier. Must be known by Batsim. Must be in the SUBMITTED state.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_reject_job(const std::string & job_id,
                                   double date) = 0;

    /**
     * @brief Appends a KILL_JOB event.
     * @param[in] job_ids The job identifiers of the jobs to kill. Must be known by Batsim.
     *                    Must be in the RUNNING state (COMPLETED jobs are ignored).
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_kill_job(const std::vector<std::string> & job_ids,
                                 double date) = 0;

    /**
     * @brief Appends a SET_RESOURCE_STATE event.
     * @param[in] resources The resources whose state must be set.
     * @param[in] new_state The state the machines should be set into.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_set_resource_state(MachineRange resources,
                                           const std::string & new_state,
                                           double date)  = 0;

    /**
     * @brief Appends a CALL_ME_LATER event.
     * @param[in] future_date The date at which the decision process shall be called.
     *            Must be greater than date.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_call_me_later(double future_date,
                                      double date) = 0;

    /**
     * @brief Appends a SUBMITER_MAY_SUBMIT_JOBS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_submitter_may_submit_jobs(double date) = 0;

    /**
     * @brief Appends a SCHEDULER_FINISHED_SUBMITTING_JOBS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_scheduler_finished_submitting_jobs(double date) = 0;

    /**
     * @brief Appends a QUERY_REQUEST event.
     * @param[in] anything ...
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_query_request(void * anything,
                                      double date) = 0;





    // Messages from Batsim to the Scheduler
    /**
     * @brief Appends a SIMULATION_STARTS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_simulation_begins(double date) = 0;

    /**
     * @brief Appends a SIMULATION_ENDS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_simulation_ends(double date) = 0;

    /**
     * @brief Appends a JOB_SUBMITTED event.
     * @param[in] job_ids The identifiers of the submitted jobs.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_job_submitted(const std::vector<std::string> & job_ids,
                                      double date) = 0;

    /**
     * @brief Appends a JOB_COMPLETED event.
     * @param[in] job_id The identifier of the job that has completed.
     * @param[in] job_status The job status
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_job_completed(const std::string & job_id,
                                      const std::string & job_status,
                                      double date) = 0;

    /**
     * @brief Appends a JOB_KILLED event.
     * @param[in] job_ids The identifiers of the jobs that have been killed.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_job_killed(const std::vector<std::string> & job_ids,
                                   double date) = 0;

    /**
     * @brief Appends a RESOURCE_STATE_CHANGED event.
     * @param[in] resources The resources whose state has changed.
     * @param[in] new_state The state the machines are now in.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_resource_state_changed(const MachineRange & resources,
                                               const std::string & new_state,
                                               double date) = 0;

    /**
     * @brief Appends a QUERY_REPLY (energy) event.
     * @param[in] consumed_energy The total consumed energy in joules
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    virtual void append_query_reply_energy(double consumed_energy,
                                           double date) = 0;

    // Management functions
    /**
     * @brief Clears inner content. Is called directly after generate_current_message.
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
     */
    JsonProtocolWriter();

    /**
     * @brief Destroys a JsonProtocolWriter
     */
    ~JsonProtocolWriter();


    // Bidirectional messages
    /**
     * @brief Appends a NOP message.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_nop(double date);

    // Messages from the Scheduler to Batsim
    /**
     * @brief Appends a SUBMIT_JOB event.
     * @details The job_description and profile_descriptions are either both given or both empty.
     *          If they are given, the job and profile information is sent within the protocol.
     *          Otherwise, it is sent by another channel (probably redis).
     * @param[in] job_id The job identifier. It must not conflict with existing job identifiers
     *            within Batism.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     * @param[in] job_description The job description string. Can be empty.
     * @param[in] profile_description The profile description string. Can be empty.
     * @param[in] acknowledge_submission Whether Batsim should acknowledge the upcoming job
     *            submission or not.
     */
    void append_submit_job(const std::string & job_id,
                           double date,
                           const std::string & job_description = "",
                           const std::string & profile_description = "",
                           bool acknowledge_submission = false);

    /**
     * @brief Appends a EXECUTE_JOB event.
     * @param[in] job_id The job identifier. It must be known by Batsim.
     * @param[in] allocated_resources The resources on which the job should be executed.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     * @param[in] executor_to_allocated_resource_mapping Optional.
     *            Allows to give a custom mapping from executors to allocated resources.
     *            By default, the number of allocated resources must equals
     *            the job size, and executor i is launched on allocated resource i.
     */
    void append_execute_job(const std::string & job_id,
                            const MachineRange & allocated_resources,
                            double date,
                            const std::string & executor_to_allocated_resource_mapping = "");

    /**
     * @brief Appends a REJECT_JOB event.
     * @param[in] job_id The job identifier. Must be known by Batsim. Must be in the SUBMITTED state.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_reject_job(const std::string & job_id,
                           double date);

    /**
     * @brief Appends a KILL_JOB event.
     * @param[in] job_ids The job identifiers of the jobs to kill. Must be known by Batsim.
     *                    Must be in the RUNNING state (COMPLETED jobs are ignored).
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_kill_job(const std::vector<std::string> & job_ids,
                         double date);

    /**
     * @brief Appends a SET_RESOURCE_STATE event.
     * @param[in] resources The resources whose state must be set.
     * @param[in] new_state The state the machines should be set into.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_set_resource_state(MachineRange resources,
                                   const std::string & new_state,
                                   double date) ;

    /**
     * @brief Appends a CALL_ME_LATER event.
     * @param[in] future_date The date at which the decision process shall be called.
     *            Must be greater than date.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_call_me_later(double future_date,
                              double date);

    /**
     * @brief Appends a SUBMITER_MAY_SUBMIT_JOBS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_submitter_may_submit_jobs(double date);

    /**
     * @brief Appends a SCHEDULER_FINISHED_SUBMITTING_JOBS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_scheduler_finished_submitting_jobs(double date);

    /**
     * @brief Appends a QUERY_REQUEST event.
     * @param[in] anything ...
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_query_request(void * anything,
                              double date);





    // Messages from Batsim to the Scheduler
    /**
     * @brief Appends a SIMULATION_STARTS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_simulation_begins(double date);

    /**
     * @brief Appends a SIMULATION_ENDS event.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_simulation_ends(double date);

    /**
     * @brief Appends a JOB_SUBMITTED event.
     * @param[in] job_ids The identifiers of the submitted jobs.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_job_submitted(const std::vector<std::string> & job_ids,
                              double date);

    /**
     * @brief Appends a JOB_COMPLETED event.
     * @param[in] job_id The identifier of the job that has completed.
     * @param[in] job_status The job status
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_job_completed(const std::string & job_id,
                              const std::string & job_status,
                              double date);

    /**
     * @brief Appends a JOB_KILLED event.
     * @param[in] job_ids The identifiers of the jobs that have been killed.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_job_killed(const std::vector<std::string> & job_ids,
                           double date);

    /**
     * @brief Appends a RESOURCE_STATE_CHANGED event.
     * @param[in] resources The resources whose state has changed.
     * @param[in] new_state The state the machines are now in.
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_resource_state_changed(const MachineRange & resources,
                                       const std::string & new_state,
                                       double date);

    /**
     * @brief Appends a QUERY_REPLY (energy) event.
     * @param[in] consumed_energy The total consumed energy in joules
     * @param[in] date The event date. Must be greater than or equal to the previous event.
     */
    void append_query_reply_energy(double consumed_energy,
                                   double date);

    // Management functions
    /**
     * @brief Clears inner content. Is called directly after generate_current_message.
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
    bool _is_empty = true;
    double _last_date = -1;
    rapidjson::Document _doc;
    rapidjson::Document::AllocatorType & _alloc;
    rapidjson::Value _events = rapidjson::Value(rapidjson::kArrayType);
};


/**
 * @brief Tests whether the JsonProtocolWriter behaves correctly
 * @return Whether the JsonProtocolWriter behaves as expected
 */
bool test_json_writer();

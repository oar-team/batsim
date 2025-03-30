/**
 * @file export.hpp
 * @brief Batsim's export classes and functions
 * @details Contains the classes and functions which are related to Batsim's exports.
 */

#pragma once

#include <stdio.h>
#include <sys/types.h> /* ssize_t, needed by xbt/str.h, included by msg/msg.h */
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <memory>

#include "pointers.hpp"
#include "machines.hpp"
#include "jobs.hpp"

struct BatsimContext;
struct Job;

/**
 * @brief Prepares Batsim's outputting
 * @param[in,out] context The BatsimContext
 */
void prepare_batsim_outputs(BatsimContext * context);

/**
 * @brief Finalizes the different Batsim's outputs
 * @param[in] context The BatsimContext
 */
void finalize_batsim_outputs(BatsimContext * context);

/**
 * @brief Buffered-write output file
 */
class WriteBuffer
{
public:
    /**
     * @brief Builds a WriteBuffer
     * @param[in] filename The file that will be written
     * @param[in] buffer_size The size of the buffer (in bytes).
     */
    explicit WriteBuffer(const std::string & filename,
                         size_t buffer_size = 64*1024);

    /**
     * @brief WriteBuffers cannot be copied.
     * @param[in] other Another instance
     */
    WriteBuffer(const WriteBuffer & other) = delete;

    /**
     * @brief Destructor
     * @details This method flushes the buffer if it is not empty, destroys the buffer and closes the file.
     */
    ~WriteBuffer();

    /**
     * @brief Appends a text at the end of the buffer. If the buffer is full, it is automatically flushed into the disk.
     * @param[in] text The text to append
     */
    void append_text(const char * text);

    /**
     * @brief Write the current content of the buffer into the file
     */
    void flush_buffer();

private:
    std::ofstream f;            //!< The file stream on which the buffer is outputted
    const size_t buffer_size;   //!< The buffer maximum size
    char * buffer = nullptr;    //!< The buffer
    size_t buffer_pos = 0;         //!< The current position of the buffer (previous positions are already written)
};


/**
 * @brief Traces how power states are changed over time
 */
class PStateChangeTracer
{
public:
    /**
     * @brief Constructs a PStateChangeTracer
     */
    PStateChangeTracer();

    /**
     * @brief PStateChangeTracer cannot be copied.
     * @param[in] other Another instance
     */
    PStateChangeTracer(const PStateChangeTracer & other) = delete;

    /**
     * @brief Destroys a PStateChangeTracer
     * @details The output file is flushed and written
     */
    ~PStateChangeTracer();

    /**
     * @brief Sets the output filename of the tracer
     * @param filename The name of the output file of the tracer
     */
    void set_filename(const std::string & filename);

    /**
     * @brief Adds a power state change in the tracer
     * @param time The time at which the change occurs
     * @param machines The machines whose state has been changed
     * @param pstate_after The power state the machine will be in after the given time
     */
    void add_pstate_change(double time, const IntervalSet & machines, int pstate_after);

    /**
     * @brief Forces the flushing of what happened to the output file
     */
    void flush();

    /**
     * @brief Closes the buffer and its associated output file
     */
    void close_buffer();

private:
    WriteBuffer * _wbuf = nullptr; //!< The buffer used to handle the output file
    char * _temporary_buffer = nullptr; //!< The buffer used to generate text
};

/**
 * @brief Traces the energy consumption of the computation machines
 */
class EnergyConsumptionTracer
{
public:
    /**
     * @brief Constructs a EnergyConsumptionTracer
     */
    EnergyConsumptionTracer() = default;

    /**
     * @brief EnergyConsumptionTracer cannot be copied.
     * @param[in] other Another instance
     */
    EnergyConsumptionTracer(const EnergyConsumptionTracer & other) = delete;

    /**
     * @brief Destroys a EnergyConsumptionTracer
     * @details The output file is flushed and written
     */
    ~EnergyConsumptionTracer();

    /**
     * @brief Sets the Batsim context
     * @param[in] context The Batsim context
     */
    void set_context(BatsimContext * context);

    /**
     * @brief Sets the output filename of the tracer
     * @param[in] filename The name of the output file of the tracer
     */
    void set_filename(const std::string & filename);

    /**
     * @brief Adds a job start in the tracer
     * @param[in] date The date at which the job has been started
     * @param[in] job_id The job unique number
     */
    void add_job_start(double date, JobIdentifier job_id);

    /**
     * @brief Adds a job end in the tracer
     * @param[in] date The date at which the job has ended
     * @param[in] job_id The job unique number
     */
    void add_job_end(double date, JobIdentifier job_id);

    /**
     * @brief Adds a power state change in the tracer
     * @param[in] date The date at which the power state has been changed
     * @param[in] machines The machines whose power state has changed
     * @param[in] new_pstate The new power state of the machine
     */
    void add_pstate_change(double date, const IntervalSet & machines, int new_pstate);

    /**
     * @brief Forces the flushing of what happened to the output file
     */
    void flush();

    /**
     * @brief Closes the buffer and its associated output file
     */
    void close_buffer();

private:
    /**
     * @brief Adds a line in the output file
     * @return The energy consumed by the computing machines from simulation's start to the current simulation time
     * @param[in] date The date at which the event has occured
     * @param[in] event_type The type of the event which occured
     */
    long double add_entry(double date, char event_type);

    long double _last_entry_date = 0; //!< The date of the last entry
    long double _last_entry_energy = 0; //!< The energy of the last entry

private:
    BatsimContext * _context = nullptr; //!< The Batsim context
    WriteBuffer * _wbuf = nullptr; //!< The buffer used to handle the output file
};

/**
 * @brief Traces the repartition of the machine states over time
 */
class MachineStateTracer
{
public:
    /**
     * @brief Constructs a MachineStateTracer
     */
    MachineStateTracer() = default;

    /**
     * @brief MachineStateTracer cannot be copied.
     * @param[in] other Another instance
     */
    MachineStateTracer(const MachineStateTracer & other) = delete;

    /**
     * @brief Destroys a MachineStateTracer
     */
    ~MachineStateTracer();

    /**
     * @brief Sets the BatsimContext
     * @param[in] context The BatsimContext
     */
    void set_context(BatsimContext * context);

    /**
     * @brief Sets the output filename of the tracer
     * @param[in] filename  The name of the output file of the tracer
     */
    void set_filename(const std::string & filename);

    /**
     * @brief Writes a line in the output file, corresponding to the current state, at the given date
     * @param[in] date The current date
     */
    void write_machine_states(double date);

    /**
     * @brief Flushes the pending writings to the output file
     */
    void flush();

    /**
     * @brief Closes the output buffer
     */
    void close_buffer();

private:
    BatsimContext * _context = nullptr; //!< The Batsim context
    WriteBuffer * _wbuf = nullptr; //!< The buffer used to handle the output file
};

/**
 * @brief Traces the jobs execution over time to export to a CSV file. Also exports schedule metrics to a second CSV file
 */
class JobsTracer
{
public:
    /**
     * @brief Constructs a JobsTracer
     */
    JobsTracer() = default;

    /**
     * @brief JobsTracer cannot be copied.
     * @param[in] other Another instance
     */
    JobsTracer(const JobsTracer & other) = delete;

    /**
     * @brief Destroys a JobsTracer
     */
    ~JobsTracer();

    /**
     * @brief Initializes the tracer
     * @param[in] context The Batsim context
     * @param[in] jobs_filename The name of the jobs output file
     * @param[in] schedule_filename The name of the schedule output file
     */
    void initialize(BatsimContext * context,
                    const std::string & jobs_filename,
                    const std::string & schedule_filename);

    /**
     * @brief Finalizes the tracer. Writes schedule output file
     */
    void finalize();

    /**
     * @brief Writes a line in the jobs output file and updates schedule metrics.
     * @param[in] job The Job involved
     */
    void write_job(const JobPtr job);

    /**
     * @brief Flushes the pending writings to the jobs output file
     */
    void flush();

    /**
     * @brief Closes the jobs output buffer
     */
    void close_buffer();


private:
    BatsimContext * _context = nullptr; //!< The Batsim context
    WriteBuffer * _wbuf = nullptr; //!< The buffer used to handle the jobs output file
    std::string _schedule_filename; //!< The filename of the schedule output file

    // Jobs-related
    std::vector<std::string> _job_keys; //!< The list of keys in the map
    std::map<std::string, std::string> _job_map; //!< The map holding info of the Job to write
    std::vector<std::string> _row_content; //!< The vector containing string to write

    // Schedule-related
    int _nb_jobs = 0; //!< The number of jobs.
    int _nb_jobs_finished = 0; //!< The number of finished jobs.
    int _nb_jobs_success = 0; //!< The number of successful jobs.
    int _nb_jobs_killed = 0; //!< The number of killed jobs.
    int _nb_jobs_rejected = 0; //!< The number of rejected jobs.
    long double _makespan = 0; //!< The makespan.
    long double _sum_waiting_time = 0; //!< The sum of the waiting time of jobs.
    long double _sum_turnaround_time = 0; //!< The sum of the turnaround time of jobs.
    long double _sum_slowdown = 0; //!< The sum of the slowdown (AKA stretch) of jobs.
    long double _max_waiting_time = 0; //!< The maximum waiting time observed.
    long double _max_turnaround_time = 0; //!< The maximum turnaround time observed.
    long double _max_slowdown = 0; //!< The maximum slowdown observed.
    std::map<int, long double> _machines_utilization; //!< Counts the utilization time of each machine.
};

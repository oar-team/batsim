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
                         int buffer_size = 64*1024);

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
    const int buffer_size;      //!< The buffer maximum size
    char * buffer = nullptr;    //!< The buffer
    int buffer_pos = 0;         //!< The current position of the buffer (previous positions are already written)
};

/**
 * @brief Exports the job execution to a CSV file
 * @param[in] filename The name of the output file used to write the CSV data
 * @param[in] context The BatsimContext
 */
void export_jobs_to_csv(const std::string &filename, const BatsimContext * context);

/**
 * @brief Compute and exports some schedule criteria to a CSV file
 * @param[in] filename The name of the output file used to write the CSV data
 * @param[in] context The BatsimContext
 */
void export_schedule_to_csv(const std::string &filename, const BatsimContext * context);


/**
 * @brief Allows to handle a Pajé trace corresponding to a schedule
 */
class PajeTracer
{
private:
    /**
     * @brief Enumerates the different states of a PajeTracer
     */
    enum PajeTracerState
    {
        UNINITIALIZED   //!< The PajeTracer has not been initialized yet
        ,INITIALIZED    //!< The PajeTracer has been initialized
        ,FINALIZED      //!< The PajeTracer has been finalized
    };

    /**
     * @brief Enumerates the constants used in the output of the Paje trace
     */
    enum PajeTracerOutputConstants
    {
        DEFINE_CONTAINER_TYPE = 1   //!< Defines a container type
        ,CREATE_CONTAINER           //!< Creates a container
        ,DESTROY_CONTAINER          //!< Destroys a container
        ,DEFINE_STATE_TYPE          //!< Defines a state type
        ,DEFINE_ENTITY_VALUE        //!< Defines an entity value
        ,SET_STATE                  //!< Sets a state
        ,DEFINE_EVENT_TYPE          //!< Defines an event type
        ,NEW_EVENT                  //!< Tells an event occured
        ,DEFINE_VARIABLE_TYPE       //!< Defines a variable type
        ,SET_VARIABLE               //!< Sets a variable
    };

public:
    /**
     * @brief Builds a PajeTracer
     * @param[in] log_launchings If set to true, job launching time will be written in the trace. This option leads to larger trace files
     */
    explicit PajeTracer(bool log_launchings = false);

    /**
     * @brief PajeTracer cannot be copied.
     * @param[in] other Another instance
     */
    PajeTracer(const PajeTracer & other) = delete;

    /**
     * @brief Sets the filename of a PajeTracer
     * @param[in] filename The name of the output file
     */
    void set_filename(const std::string & filename);

    /**
     * @brief PajeTracer destructor.
     */
    ~PajeTracer();

    /**
     * @brief Initializes a PajeTracer.
     * @details This function must be called once before adding job launchings, runnings or endings.
     * @param[in] context The BatsimContext
     * @param[in] time The beginning time
     */
    void initialize(const BatsimContext * context, double time);

    /**
     * @brief Finalizes a PajeTracer.
     * @details This function must be called before the PajeTracer's object destruction.
     * @param[in] context The BatsimContext
     * @param[in] time The simulation time at which the finalization is done
     */
    void finalize(const BatsimContext * context, double time);

    /**
     * @brief Adds a job launch in the file trace.
     * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet.
     * @param[in] job The job
     * @param[in] used_machine_ids The machines which compute the job
     * @param[in] time The simulation time at which the addition is done
     */
    void add_job_launching(const Job * job, const std::vector<int> & used_machine_ids, double time);

    /**
     * @brief Creates a job in the Pajé output file
     * @param[in] job The job
     */
    void register_new_job(const Job * job);

    /**
     * @brief Sets a machine in the idle state
     * @param[in] machine_id The unique machine number
     * @param[in] time The time at which the machine should be marked as idle
     */
    void set_machine_idle(int machine_id, double time);

    /**
     * @brief Sets a machine in the computing state
     * @param[in] machine_id The unique machine number
     * @param[in] job The job
     * @param[in] time The time at which the machine should be marked as computing the job
     */
    void set_machine_as_computing_job(int machine_id, const Job * job, double time);

    /**
     * @brief Adds a job kill in the file trace.
     * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet.
     * @param[in] job The job
     * @param[in] used_machine_ids The machines which compute the job
     * @param[in] time The simulation time at which the kill is done
     * @param[in] associate_kill_to_machines By default (false), one event is added in the killer container. If set to true, one event is added for every machine on which the kill occurs.
     */
    void add_job_kill(const Job * job, const IntervalSet & used_machine_ids,
                      double time, bool associate_kill_to_machines = false);

public:
    /**
     * @brief Give the RGB representation of a color represented in HSV
     * @details This function is greatly inspired by http://www.cs.rit.edu/~ncs/color/t_convert.html
     * @param[in] h The hue, whose value is in [0,360]
     * @param[in] s The saturation, whose value is in [0,1]
     * @param[in] v The value, whose value is in [0,1]
     * @param[out] r The red, whose value is in [0,1]
     * @param[out] g The green, whose value is in [0,1]
     * @param[out] b The blue, whose value is in [0,1]
     */
    static void hsv_to_rgb(double h, double s, double v, double & r, double & g, double & b);

private:
    /**
     * @brief Generate colors
     * @details The colors are fairly shared in the Hue color spectrum.
     * @param[in] color_count The number of colours to generate
     */
    void generate_colors(int color_count = 8);

    /**
     * @brief Randomize the position of the colors in the colormap
     */
    void shuffle_colors();

private:
    const char * rootType = "root_ct";                  //!< The root type output name
    const char * machineType = "machine_ct";            //!< The machine type output name
    const char * machineState = "machine_state";        //!< The machine state output name
    const char * schedulerType = "scheduler_ct";        //!< The scheduler type output name
    const char * killerType = "killer_ct";              //!< The killer type output name
    const char * killEventKiller = "kk";                //!< The kill event (on the Killer) output name
    const char * killEventMachine = "km";               //!< The kill event (on machines) output name
    const char * utilizationVarType = "vu_vt";          //!< The utilization variable type output name

    const char * mstateWaiting = "w";                   //!< The waiting state output name
    const char * mstateLaunching = "l";                 //!< The launching state output name

    const char * root = "root";                         //!< The Pajé root output name
    const char * scheduler = "sc";                      //!< The scheduler output name
    const char * killer = "k";                          //!< The killer output name
    const char * machinePrefix = "m";                   //!< The machine output prefix
    const char * jobPrefix = "j";                       //!< The job output prefix
    const char * waitingColor= "\"0.0 0.0 0.0\"";       //!< The color used for idle machines
    const char * launchingColor = "\"0.3 0.3 0.3\"";    //!< The color used for machines launching a job
    const char * utilizationColor = "\"0.0 0.5 0.0\"";  //!< The color used for the utilization graph

    const bool _log_launchings; //!< If set to true, job launchings should be outputted (in addition to starting decisions)

    WriteBuffer * _wbuf = nullptr;  //!< The buffer class used to handle the output file

    std::map<const Job *, std::string> _jobs; //!< Maps jobs to their Pajé representation
    std::vector<std::string> _colors; //!< Strings associated with colors, used for the jobs

    PajeTracerState state = UNINITIALIZED; //!< The state of the PajeTracer
    int nb_total_jobs = 0;      //!< The total number of jobs added to Paje
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
    void setFilename(const std::string & filename);

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
    void write_job(const Job * job);

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
    int _nb_jobs = 0;
    int _nb_jobs_finished = 0;
    int _nb_jobs_success = 0;
    int _nb_jobs_killed = 0;
    long double _makespan = 0;
    long double _sum_waiting_time = 0;
    long double _sum_turnaround_time = 0;
    long double _sum_slowdown = 0;
    long double _max_waiting_time = 0;
    long double _max_turnaround_time = 0;
    long double _max_slowdown = 0;
    std::map<int, long double> _machines_utilization;
};

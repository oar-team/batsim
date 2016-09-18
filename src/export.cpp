/**
 * @file export.cpp
 * @brief Batsim's export classes and functions
 * @details Contains the classes and functions which are related to Batsim's exports.
 */

#include "export.hpp"

#include <algorithm>
#include <fstream>

#include <boost/algorithm/string/join.hpp>

#include <stdlib.h>
#include <xbt.h>
#include <math.h>
#include <float.h>

#include "context.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(export, "export"); //!< Logging


void prepare_batsim_outputs(BatsimContext * context)
{
    if (context->trace_schedule)
    {
        context->paje_tracer.set_filename(context->export_prefix + "_schedule.trace");
        context->machines.set_tracer(&context->paje_tracer);
        context->paje_tracer.initialize(context, MSG_get_clock());
    }

    if (context->energy_used)
    {
        // Energy consumption tracing
        context->energy_tracer.set_context(context);
        context->energy_tracer.set_filename(context->export_prefix + "_consumed_energy.csv");

        // Power state tracing
        context->pstate_tracer.setFilename(context->export_prefix + "_pstate_changes.csv");

        std::map<int, MachineRange> pstate_to_machine_set;
        for (const Machine * machine : context->machines.machines())
        {
            int machine_id = machine->id;
            int pstate = MSG_host_get_pstate(machine->host);

            if (pstate_to_machine_set.count(pstate) == 0)
            {
                MachineRange range;
                range.insert(machine_id);
                pstate_to_machine_set[pstate] = range;
            }
            else
                pstate_to_machine_set[pstate].insert(machine_id);
        }

        for (auto mit : pstate_to_machine_set)
        {
            int pstate = mit.first;
            MachineRange & range = mit.second;
            context->pstate_tracer.add_pstate_change(MSG_get_clock(), range, pstate);
        }
    }
}

void finalize_batsim_outputs(BatsimContext * context)
{
    // Let's say the simulation is ended now
    context->simulation_end_time = chrono::high_resolution_clock::now();

    // Schedule (Pajé)
    if (context->trace_schedule)
        context->paje_tracer.finalize(context, MSG_get_clock());

    // Schedule-oriented output information
    export_schedule_to_csv(context->export_prefix + "_schedule.csv", context);

    // Job-oriented output information
    export_jobs_to_csv(context->export_prefix + "_jobs.csv", context);
}


WriteBuffer::WriteBuffer(const std::string & filename, int bufferSize)
    : bufferSize(bufferSize)
{
    xbt_assert(bufferSize > 0, "Invalid buffer size (%d)", bufferSize);
    buffer = new char[bufferSize];

    f.open(filename, ios_base::trunc);
    xbt_assert(f.is_open(), "Cannot write file '%s'", filename.c_str());
}

WriteBuffer::~WriteBuffer()
{
    if (bufferPos > 0)
        flushBuffer();

    if (buffer != nullptr)
    {
        delete[] buffer;
        buffer = nullptr;

        f.close();
    }
}

void WriteBuffer::appendText(const char * text)
{
    const int textLength = strlen(text);

    // If the buffer is big enough
    if (bufferPos + textLength < bufferSize)
    {
        // Let's append the text to the buffer
        memcpy(buffer + bufferPos, text, textLength * sizeof(char));
        bufferPos += textLength;
    }
    else
    {
        // Let's write the current buffer content in the file
        flushBuffer();

        // Let's write the text in the buffer
        xbt_assert(textLength < bufferSize, "Text too large to fit in the buffer (%d required, only got %d)", textLength, bufferSize);
        memcpy(buffer, text, textLength * sizeof(char));
        bufferPos = textLength;
    }
}

void WriteBuffer::flushBuffer()
{
    f.write(buffer, bufferPos);
    bufferPos = 0;
}





PajeTracer::PajeTracer(bool log_launchings) :
    _log_launchings(log_launchings)
{
    generate_colors(64);
    shuffle_colors();
}

void PajeTracer::set_filename(const string &filename)
{
    xbt_assert(_wbuf == nullptr, "Double call of PajeTracer::set_filename");
    _wbuf = new WriteBuffer(filename);
}

PajeTracer::~PajeTracer()
{
    // If the write buffer had not been set, the PajeTracer has not been used and can disappear in silence
    if (_wbuf != nullptr)
    {
        if (state != FINALIZED && state != UNINITIALIZED)
            fprintf(stderr, "Destruction of a PajeTracer object which has not been finalized. "
                            "The corresponding trace file may be invalid.\n");

        if (_wbuf != nullptr)
        {
            delete _wbuf;
            _wbuf = nullptr;
        }
    }
}

void PajeTracer::initialize(const BatsimContext *context, double time)
{
    xbt_assert(state == UNINITIALIZED, "Bad PajeTracer::initialize call: the object is not UNINITIALIZED");

    // The buffer used to write lines in the file
    const int bufSize = 1024;
    char buf[bufSize];

    // Let's write the Pajé schedule header
    // Let's write the Pajé schedule header
    snprintf(buf, bufSize,
             "%%EventDef PajeDefineContainerType %d\n"
             "%% Type string\n"
             "%% Alias string\n"
             "%% Name string\n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeCreateContainer %d\n"
             "%% Time date \n"
             "%% Type string  \n"
             "%% Alias string \n"
             "%% Name string   \n"
             "%% Container string  \n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeDestroyContainer %d\n"
             "%% Time date\n"
             "%% Name string \n"
             "%% Type string\n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeDefineStateType %d\n"
             "%% Alias string \n"
             "%% Type string \n"
             "%% Name string \n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeDefineEntityValue %d\n"
             "%% Alias string  \n"
             "%% Type string  \n"
             "%% Name string  \n"
             "%% Color color \n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeSetState %d\n"
             "%% Time date  \n"
             "%% Type string  \n"
             "%% Container string  \n"
             "%% Value string  \n"
             "%%EndEventDef \n"
             "\n"
             "%%EventDef PajeDefineEventType %d\n"
             "%% Type string\n"
             "%% Alias string\n"
             "%% Name string\n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeNewEvent %d\n"
             "%% Time date\n"
             "%% Type string\n"
             "%% Container string\n"
             "%% Value string\n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeDefineVariableType %d\n"
             "%% Type string\n"
             "%% Alias string\n"
             "%% Name string\n"
             "%% Color string\n"
             "%%EndEventDef\n"
             "\n"
             "%%EventDef PajeSetVariable %d\n"
             "%% Time date\n"
             "%% Type string\n"
             "%% Container string\n"
             "%% Value double\n"
             "%%EndEventDef\n"
             "\n",
             DEFINE_CONTAINER_TYPE, CREATE_CONTAINER, DESTROY_CONTAINER,
             DEFINE_STATE_TYPE, DEFINE_ENTITY_VALUE, SET_STATE,
             DEFINE_EVENT_TYPE, NEW_EVENT, DEFINE_VARIABLE_TYPE,
             SET_VARIABLE);
    _wbuf->appendText(buf);

    // Let's create our container types
    snprintf(buf, bufSize,
             "# Container types creation\n"
             "%d 0 %s \"Machines\"\n"
             "%d %s %s \"Machine\"\n"
             "%d 0 %s \"Scheduler\"\n"
             "%d %s %s \"Killer\"\n"
             "\n",
             DEFINE_CONTAINER_TYPE,                rootType,
             DEFINE_CONTAINER_TYPE, rootType,      machineType,
             DEFINE_CONTAINER_TYPE,                schedulerType,
             DEFINE_CONTAINER_TYPE, schedulerType, killerType);
    _wbuf->appendText(buf);

    // Let's create our event types
    snprintf(buf, bufSize,
             "# Event types creation\n"
             "%d %s %s \"Job kill\"\n"
             "%d %s %s \"Job kill\"\n"
             "\n",
             DEFINE_EVENT_TYPE, killerType, killEventKiller,
             DEFINE_EVENT_TYPE, machineType, killEventMachine);
    _wbuf->appendText(buf);

    // Let's create our variable types
    snprintf(buf, bufSize,
             "# Variable types creation\n"
             "%d %s %s \"Utilization\" %s\n"
             "\n",
             DEFINE_VARIABLE_TYPE, schedulerType, utilizationVarType, utilizationColor);
    _wbuf->appendText(buf);

    snprintf(buf, bufSize,
             "# Containers creation\n"
             "%d %lf %s %s \"Machines\" 0\n",
             CREATE_CONTAINER, time, rootType, root);
    _wbuf->appendText(buf);

    for (const Machine * m : context->machines.machines())
    {
        // todo : clean machine name
        snprintf(buf, bufSize,
                 "%d %lf %s %s%d \"%s\" %s\n",
                 CREATE_CONTAINER, time, machineType,
                 machinePrefix, m->id,
                 m->name.c_str(), root);
        _wbuf->appendText(buf);
    }

    snprintf(buf, bufSize,
             "%d %lf %s %s \"Scheduler\" 0\n"
             "%d %lf %s %s \"Killer\" %s\n"
             "\n",
             CREATE_CONTAINER, time, schedulerType, scheduler,
             CREATE_CONTAINER, time, killerType, killer, scheduler);
    _wbuf->appendText(buf);

    // Let's declare that machines have a state
    snprintf(buf, bufSize,
             "# States creation\n"
             "%d %s %s \"Machine state\"\n"
             "\n",
             DEFINE_STATE_TYPE, machineState, machineType);
    _wbuf->appendText(buf);

    // Let's declare some machine states
    snprintf(buf, bufSize,
             "# Creation of the different values the machine state can be\n"
             "%d %s %s \"Waiting\" %s\n"
             "%d %s %s \"Launching\" %s\n"
             "\n"
             "# Begin of events\n",
             DEFINE_ENTITY_VALUE, mstateWaiting, machineState, waitingColor,
             DEFINE_ENTITY_VALUE, mstateLaunching, machineState, launchingColor);
    _wbuf->appendText(buf);

    // Let's set all the machines in waiting state
    for (const Machine * m : context->machines.machines())
    {
        snprintf(buf, bufSize,
                 "%d %lf %s %s%d %s\n",
                 SET_STATE, time, machineState, machinePrefix, m->id, mstateWaiting);
        _wbuf->appendText(buf);
    }

    state = INITIALIZED;
}

void PajeTracer::finalize(const BatsimContext * context, double time)
{
    xbt_assert(state == INITIALIZED, "Bad PajeTracer::finalize call: the object has not been initialized yet");

    const int bufSize = 64;
    char buf[bufSize];

    snprintf(buf, bufSize,
             "\n"
             "# End of events, containers destruction\n");
    _wbuf->appendText(buf);

    for (const Machine * m : context->machines.machines())
    {
        snprintf(buf, bufSize,
                 "%d %lf %s%d %s\n",
                 DESTROY_CONTAINER, time, machinePrefix, m->id, machineType);
        _wbuf->appendText(buf);
    }

    snprintf(buf, bufSize,
             "%d %lf %s %s\n",
             DESTROY_CONTAINER, time, root, rootType);
    _wbuf->appendText(buf);
    _wbuf->flushBuffer();

    state = FINALIZED;

    XBT_INFO("PajeTracer finalized");
}

void PajeTracer::add_job_launching(int jobID, const std::vector<int> & usedMachineIDs, double time)
{
    (void) jobID;
    xbt_assert(state == INITIALIZED, "Bad addJobLaunching call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    if (_log_launchings)
    {
        // Let's change the state of all the machines which launch the job
        for (const int & machineID : usedMachineIDs)
        {
            snprintf(buf, bufSize,
                     "%d %lf %s %s%d %s\n",
                     SET_STATE, time, machineState, machinePrefix, machineID, mstateLaunching);
            _wbuf->appendText(buf);
        }
    }
}

void PajeTracer::register_new_job(int jobID)
{
    xbt_assert(_jobs.find(jobID) == _jobs.end(), "Cannot register new job %d: it already exists", jobID);
    const int bufSize = 64;
    char buf[bufSize];

    // Let's create a state value corresponding to this job
    snprintf(buf, bufSize,
             "%d %s%d %s \"%d\" %s\n",
             DEFINE_ENTITY_VALUE, jobPrefix, jobID, machineState, jobID, _colors[jobID % (int)_colors.size()].c_str());
    _wbuf->appendText(buf);

    _jobs[jobID] = jobPrefix + to_string(jobID);
}

void PajeTracer::set_machine_idle(int machineID, double time)
{
    const int bufSize = 64;
    char buf[bufSize];
    snprintf(buf, bufSize,
             "%d %lf %s %s%d %s\n",
             SET_STATE, time, machineState, machinePrefix, machineID, mstateWaiting);
    _wbuf->appendText(buf);
}

void PajeTracer::set_machine_as_computing_job(int machineID, int jobID, double time)
{
    auto mit = _jobs.find(jobID);
    if (mit == _jobs.end())
    {
        register_new_job(jobID);
        mit = _jobs.find(jobID);
    }

    const int bufSize = 64;
    char buf[bufSize];
    snprintf(buf, bufSize,
             "%d %lf %s %s%d %s\n",
             SET_STATE, time, machineState, machinePrefix, machineID, mit->second.c_str());
    _wbuf->appendText(buf);
}

void PajeTracer::add_job_kill(int jobID, const MachineRange & usedMachineIDs, double time, bool associateKillToMachines)
{
    xbt_assert(state == INITIALIZED, "Bad addJobKill call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    // Let's add a kill event associated with the scheduler
    snprintf(buf, bufSize,
             "%d %lf %s %s \"%d\"\n",
             NEW_EVENT, time, killEventKiller, killer, jobID);
    _wbuf->appendText(buf);

    if (associateKillToMachines)
    {
        // Let's add a kill event associated with each machine
        for (auto it = usedMachineIDs.elements_begin(); it != usedMachineIDs.elements_end(); ++it)
        {
            int machine_id = *it;
            snprintf(buf, bufSize,
                     "%d %lf %s %s%d \"%d\"\n",
                     NEW_EVENT, time, killEventMachine, machinePrefix, machine_id, jobID);
            _wbuf->appendText(buf);
        }
    }
}

void PajeTracer::add_global_utilization(double utilization, double time)
{
    xbt_assert(state == INITIALIZED, "Bad addJobKill call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    // Let's set the variable state correctly
    snprintf(buf, bufSize,
             "%d %lf %s %s %lf\n",
             SET_VARIABLE, time, utilizationVarType, scheduler, utilization);
    _wbuf->appendText(buf);
}

void PajeTracer::generate_colors(int colorCount)
{
    xbt_assert(colorCount > 0);

    double h, s=1, v=1, r, g, b;
    const int bufSize = 32;
    char buf[bufSize];

    double hueFraction = 360.0 / colorCount;
    for (int i = 0; i < colorCount; ++i)
    {
        h = i * hueFraction;
        hsv_to_rgb(h,s,v, r,g,b);

        snprintf(buf, bufSize, "\"%lf %lf %lf\"", r, g, b);
        _colors.push_back(buf);
    }
}

void PajeTracer::shuffle_colors()
{
    random_shuffle(_colors.begin(), _colors.end());
}

void PajeTracer::hsv_to_rgb(double h, double s, double v, double & r, double & g, double & b)
{
    if (s == 0) // Achromatic (grey)
    {
        r = g = b = v;
        return;
    }

    h /= 60;            // sector 0 to 5
    int i = floor(h);
    float f = h-i;      // factorial part of h
    float p = v*(1-s);
    float q = v*(1-s*f);
    float t = v*(1-s*(1-f));

    switch(i)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:    // case 5:
        r = v;
        g = p;
        b = q;
        break;
    }
}


void export_jobs_to_csv(const std::string &filename, const BatsimContext *context)
{
    ofstream f(filename, ios_base::trunc);
    xbt_assert(f.is_open(), "Cannot write file '%s'", filename.c_str());

    // write headers
    f << "jobID,workload_name,submission_time,requested_number_of_processors,requested_time,success,starting_time,execution_time,finish_time,waiting_time,turnaround_time,stretch,consumed_energy,allocated_processors\n";

    for (const auto mit : context->workloads.workloads())
    {
        string workload_name = mit.first;
        const Workload * workload = mit.second;
        int workload_num = distance((context->workloads.workloads()).begin(), (context->workloads.workloads()).find(mit.first));

        if(workload->jobs)
        {
            const auto & jobs = workload->jobs->jobs();
            for (const auto & mit : jobs)
            {
                Job * job = mit.second;

                if (job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY || job->state == JobState::JOB_STATE_COMPLETED_KILLED)
                {
                    char * buf = nullptr;
                    int success = (job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY);
                    xbt_assert(job->runtime >= 0);

                    int ret = asprintf(&buf, "%d,%s,%lf,%d,%lf,%d,%lf,%lf,%lf,%lf,%lf,%lf,%Lf,", // finished by a ',' because the next part is written after asprintf
                                       job->number*10 + workload_num, // job_id
                                       workload_name.c_str(), // workload_name
                                       job->submission_time, // submission_time
                                       job->required_nb_res, // requested_number_of_processors
                                       job->walltime, // requested_time
                                       success, // success
                                       job->starting_time, // starting_time
                                       job->runtime, // execution_time
                                       job->starting_time + job->runtime, // finish_time
                                       job->starting_time - job->submission_time, // waiting_time
                                       job->starting_time + job->runtime - job->submission_time, // turnaround_time
                                       (job->starting_time + job->runtime - job->submission_time) / job->runtime, // stretch
                                       job->consumed_energy // consumed energy
                                       );
                    (void) ret; // Avoids a warning if assertions are ignored
                    xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                    f << buf;
                    free(buf);

                    xbt_assert((int)job->allocation.size() == job->required_nb_res);

                    f << job->allocation.to_string_hyphen(" ") << "\n";
                }
            }
        }
    }

    f.close();
}


void export_schedule_to_csv(const std::string &filename, const BatsimContext *context)
{
    ofstream f(filename, ios_base::trunc);
    xbt_assert(f.is_open(), "Cannot write file '%s'", filename.c_str());

    f << "nb_jobs,nb_jobs_finished,nb_jobs_success,nb_jobs_killed,success_rate,makespan,max_turnaround_time,simulation_time,scheduling_time,jobs_execution_time_boundary_ratio,consumed_joules\n";

    int nb_jobs = 0;
    int nb_jobs_finished = 0;
    int nb_jobs_success = 0;
    int nb_jobs_killed = 0;
    double makespan = 0;
    double max_turnaround_time = 0;
    double min_job_execution_time = DBL_MAX;
    double max_job_execution_time = DBL_MIN;

    long double seconds_used_by_scheduler = context->microseconds_used_by_scheduler / (long double)1e6;

    // Let's compute the simulation time
    chrono::duration<long double> diff = context->simulation_end_time - context->simulation_start_time;
    long double seconds_used_by_the_whole_simulation = diff.count();

    for (const auto mit : context->workloads.workloads())
    {
        const Workload * workload = mit.second;

    if(workload->jobs)
      {
        const auto & jobs = workload->jobs->jobs();
        for (const auto & mit : jobs)
        {
            Job * job = mit.second;

            if (job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY || job->state == JobState::JOB_STATE_COMPLETED_KILLED)
            {
                nb_jobs_finished++;

                if (job->runtime < min_job_execution_time)
                    min_job_execution_time = job->runtime;
                if (job->runtime > max_job_execution_time)
                    max_job_execution_time = job->runtime;

                if (job->state == JobState::JOB_STATE_COMPLETED_SUCCESSFULLY)
                    nb_jobs_success++;
                else
                    nb_jobs_killed++;

                double completion_time = job->starting_time + job->runtime;
                double turnaround_time = job->starting_time + job->runtime - job->submission_time;

                if (completion_time > makespan)
                    makespan = completion_time;

                if (turnaround_time > max_turnaround_time)
                    max_turnaround_time = turnaround_time;
            }
        }
      }
    }

    XBT_INFO("Makespan=%lf, scheduling_time=%Lf", makespan, seconds_used_by_scheduler);

    long double total_consumed_energy = context->energy_last_job_completion - context->energy_first_job_submission;

    char * buf;
    int ret = asprintf(&buf, "%d,%d,%d,%d,%lf,%lf,%lf,%Lf,%Lf,%lf,%Lg\n",
                       nb_jobs, nb_jobs_finished, nb_jobs_success, nb_jobs_killed,
                       (double)nb_jobs_success/nb_jobs, makespan, max_turnaround_time,
                       seconds_used_by_the_whole_simulation,
                       seconds_used_by_scheduler,
                       max_job_execution_time / min_job_execution_time,
                       total_consumed_energy);
    (void) ret; // Avoids a warning if assertions are ignored
    xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

    f << buf;
    free(buf);
}

PStateChangeTracer::PStateChangeTracer()
{

}

void PStateChangeTracer::setFilename(const string &filename)
{
    xbt_assert(_wbuf == nullptr, "Double call of PStateChangeTracer::setFilename");
    _wbuf = new WriteBuffer(filename);

    _wbuf->appendText("time,machine_id,new_pstate\n");
}

PStateChangeTracer::~PStateChangeTracer()
{
    if (_wbuf != nullptr)
    {
        delete _wbuf;
        _wbuf = nullptr;
    }
}

void PStateChangeTracer::add_pstate_change(double time, MachineRange machines, int pstate_after)
{
    const int bufSize = 64;
    char buf[bufSize];

    snprintf(buf, bufSize, "%g,%s,%d\n",
             time, machines.to_string_hyphen(" ", "-").c_str(), pstate_after);
    xbt_assert(_wbuf != nullptr);
    _wbuf->appendText(buf);
}


EnergyConsumptionTracer::EnergyConsumptionTracer()
{
}

EnergyConsumptionTracer::~EnergyConsumptionTracer()
{
    if (_wbuf != nullptr)
    {
        delete _wbuf;
        _wbuf = nullptr;
    }
}

void EnergyConsumptionTracer::set_context(BatsimContext *context)
{
    xbt_assert(context->energy_used);
    _context = context;
}

void EnergyConsumptionTracer::set_filename(const string &filename)
{
    xbt_assert(_wbuf == nullptr, "Double call of EnergyConsumptionTracer::set_filename");
    _wbuf = new WriteBuffer(filename);

    _wbuf->appendText("time,energy,event_type\n");
}

void EnergyConsumptionTracer::add_job_start(double date, int job_id)
{
    (void) job_id;
    add_entry(date, 's');
}

void EnergyConsumptionTracer::add_job_end(double date, int job_id)
{
    (void) job_id;
    _context->energy_last_job_completion = add_entry(date, 'e');
}

void EnergyConsumptionTracer::add_pstate_change(double date, const MachineRange & machines, int new_pstate)
{
    (void) machines;
    (void) new_pstate;
    add_entry(date, 'p');
}

long double EnergyConsumptionTracer::add_entry(double date, char event_type)
{
    xbt_assert(_wbuf != nullptr);

    long double energy = _context->machines.total_consumed_energy(_context);

    const int buf_size = 64;
    char buf[buf_size];

    snprintf(buf, buf_size, "%g,%Lg,%c\n",
             date, energy, event_type);
    xbt_assert(_wbuf != nullptr);
    _wbuf->appendText(buf);

    return energy;
}

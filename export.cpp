#include "export.hpp"

#include <algorithm>
#include <stdlib.h>
#include <xbt.h>
#include <math.h>
#include <float.h>

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(export, "export");

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





PajeTracer::PajeTracer(const std::string & filename,
                       bool logLaunchings) :
    logLaunchings(logLaunchings)
{
    generateColors(64);
    shuffleColors();

    wbuf = new WriteBuffer(filename);
}

PajeTracer::~PajeTracer()
{
    if (state != FINALIZED)
        fprintf(stderr, "Destruction of a PajeTracer object which has not been finalized. The corresponding trace file may be invalid.");

    if (wbuf != nullptr)
    {
        delete wbuf;
        wbuf = nullptr;
    }
}

void PajeTracer::initialize(const vector<Machine> & machines, double time)
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
    wbuf->appendText(buf);

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
    wbuf->appendText(buf);

    // Let's create our event types
    snprintf(buf, bufSize,
        "# Event types creation\n"
        "%d %s %s \"Job kill\"\n"
        "%d %s %s \"Job kill\"\n"
        "\n",
        DEFINE_EVENT_TYPE, killerType, killEventKiller,
        DEFINE_EVENT_TYPE, machineType, killEventMachine);
    wbuf->appendText(buf);

    // Let's create our variable types
    snprintf(buf, bufSize,
        "# Variable types creation\n"
        "%d %s %s \"Utilization\" %s\n"
        "\n",
        DEFINE_VARIABLE_TYPE, schedulerType, utilizationVarType, utilizationColor);
    wbuf->appendText(buf);

    snprintf(buf, bufSize,
        "# Containers creation\n"
        "%d %lf %s %s \"Machines\" 0\n",
        CREATE_CONTAINER, time, rootType, root);
    wbuf->appendText(buf);

    for (const Machine & m : machines)
    {
        // todo : clean machine name
        snprintf(buf, bufSize,
            "%d %lf %s %s%d \"%s\" %s\n",
            CREATE_CONTAINER, time, machineType,
            machinePrefix, m.id,
            m.name.c_str(), root);
        wbuf->appendText(buf);
    }

    snprintf(buf, bufSize,
        "%d %lf %s %s \"Scheduler\" 0\n"
        "%d %lf %s %s \"Killer\" %s\n"
        "\n",
        CREATE_CONTAINER, time, schedulerType, scheduler,
        CREATE_CONTAINER, time, killerType, killer, scheduler);
    wbuf->appendText(buf);

    // Let's declare that machines have a state
    snprintf(buf, bufSize,
        "# States creation\n"
        "%d %s %s \"Machine state\"\n"
        "\n",
        DEFINE_STATE_TYPE, machineState, machineType);
    wbuf->appendText(buf);

    // Let's declare some machine states
    snprintf(buf, bufSize,
        "# Creation of the different values the machine state can be\n"
        "%d %s %s \"Waiting\" %s\n"
        "%d %s %s \"Launching\" %s\n"
        "\n"
        "# Begin of events\n",
        DEFINE_ENTITY_VALUE, mstateWaiting, machineState, waitingColor,
        DEFINE_ENTITY_VALUE, mstateLaunching, machineState, launchingColor);
    wbuf->appendText(buf);

    // Let's set all the machines in waiting state
    for (const Machine & m : machines)
    {
        snprintf(buf, bufSize,
            "%d %lf %s %s%d %s\n",
            SET_STATE, time, machineState, machinePrefix, m.id, mstateWaiting);
        wbuf->appendText(buf);
    }

    state = INITIALIZED;
}

void PajeTracer::finalize(const vector<Machine> & machines, double time)
{
    xbt_assert(state == INITIALIZED, "Bad PajeTracer::finalize call: the object has not been initialized yet");

    const int bufSize = 64;
    char buf[bufSize];

    snprintf(buf, bufSize,
        "\n"
        "# End of events, containers destruction\n");
    wbuf->appendText(buf);

    for (const Machine & m : machines)
    {
        snprintf(buf, bufSize,
            "%d %lf %s%d %s\n",
            DESTROY_CONTAINER, time, machinePrefix, m.id, machineType);
        wbuf->appendText(buf);
    }

    snprintf(buf, bufSize,
        "%d %lf %s %s\n",
            DESTROY_CONTAINER, time, root, rootType);
    wbuf->appendText(buf);

    state = FINALIZED;
}

void PajeTracer::addJobLaunching(int jobID, const std::vector<int> & usedMachineIDs, double time)
{
    xbt_assert(state == INITIALIZED, "Bad addJobLaunching call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    // Let's create a state value corresponding to this job
    snprintf(buf, bufSize,
        "%d %s%d %s \"%d\" %s\n",
        DEFINE_ENTITY_VALUE, jobPrefix, jobID, machineState, jobID, colors[jobID % (int)colors.size()].c_str());
    wbuf->appendText(buf);

    if (logLaunchings)
    {
        // Let's change the state of all the machines which launch the job
        for (const int & machineID : usedMachineIDs)
        {
            snprintf(buf, bufSize,
                "%d %lf %s %s%d %s\n",
                SET_STATE, time, machineState, machinePrefix, machineID, mstateLaunching);
            wbuf->appendText(buf);
        }
    }

}

void PajeTracer::addJobRunning(int jobID, const vector<int> & usedMachineIDs, double time)
{
    xbt_assert(state == INITIALIZED, "Bad addJobLaunching call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    // Let's change the state of all the machines which run the job
    for (const int & machineID : usedMachineIDs)
    {
        snprintf(buf, bufSize,
            "%d %lf %s %s%d %s%d\n",
            SET_STATE, time, machineState, machinePrefix, machineID, jobPrefix, jobID);
        wbuf->appendText(buf);
    }
}

void PajeTracer::addJobEnding(int jobID, const vector<int> & usedMachineIDs, double time)
{
    xbt_assert(state == INITIALIZED, "Bad addJobLaunching call: the PajeTracer object is not initialized or had been finalized");
    (void) jobID;

    const int bufSize = 64;
    char buf[bufSize];

    // Let's change the state of all the machines which run the job
    for (const int & machineID : usedMachineIDs)
    {
        snprintf(buf, bufSize,
            "%d %lf %s %s%d %s\n",
            SET_STATE, time, machineState, machinePrefix, machineID, mstateWaiting);
        wbuf->appendText(buf);
    }
}

void PajeTracer::addJobKill(int jobID, const vector<int> & usedMachineIDs, double time)
{
    xbt_assert(state == INITIALIZED, "Bad addJobKill call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    // Let's add a kill event associated with the scheduler
    snprintf(buf, bufSize,
        "%d %lf %s %s \"%d\"\n",
        NEW_EVENT, time, killEventKiller, killer, jobID);
    wbuf->appendText(buf);

    // Let's add a kill event associated with each machine
    for (const int & machineID : usedMachineIDs)
    {
        snprintf(buf, bufSize,
            "%d %lf %s %s%d \"%d\"\n",
            NEW_EVENT, time, killEventMachine, machinePrefix, machineID, jobID);
        wbuf->appendText(buf);
    }
}

void PajeTracer::addGlobalUtilization(double utilization, double time)
{
    xbt_assert(state == INITIALIZED, "Bad addJobKill call: the PajeTracer object is not initialized or had been finalized");

    const int bufSize = 64;
    char buf[bufSize];

    // Let's set the variable state correctly
    snprintf(buf, bufSize,
        "%d %lf %s %s %lf\n",
        SET_VARIABLE, time, utilizationVarType, scheduler, utilization);
    wbuf->appendText(buf);
}

void PajeTracer::generateColors(int colorCount)
{
    xbt_assert(colorCount > 0);

    double h, s=1, v=1, r, g, b;
    const int bufSize = 32;
    char buf[bufSize];

    double hueFraction = 360.0 / colorCount;
    for (int i = 0; i < colorCount; ++i)
    {
        h = i * hueFraction;
        hsvToRgb(h,s,v, r,g,b);

        snprintf(buf, bufSize, "\"%lf %lf %lf\"", r, g, b);
        colors.push_back(buf);
    }
}

void PajeTracer::shuffleColors()
{
    random_shuffle(colors.begin(), colors.end());
}

void PajeTracer::hsvToRgb(double h, double s, double v, double & r, double & g, double & b)
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

void exportJobsToCSV(const char *filename)
{

}
/* TODO
void exportJobsToCSV(const char *filename)
{
    FILE * f = fopen(filename, "w");

    // jobID
    // submit time
    // number of processors
    // rejected (0 or 1)
    // wall time
    // starting time
    // execution time
    // used processors (list of ranges)

    if (f != NULL)
    {
        fputs("jobID,submission_time,requested_number_of_processors,requested_time,success,starting_time,execution_time,finish_time,waiting_time,turnaround_time,stretch,allocated_processors\n", f);

        if (jobs_dynar != NULL)
        {
            unsigned int i;
            s_job_t * job;
            char * buf;

            xbt_dynar_foreach(jobs_dynar, i, job)
            {
                if (job->state == JOB_STATE_COMPLETED_SUCCESSFULLY || job->state == JOB_STATE_COMPLETED_KILLED)
                {
                    int success = job->state == JOB_STATE_COMPLETED_SUCCESSFULLY;
                    int ret = asprintf(&buf, "%d,%lf,%d,%lf,%d,%lf,%lf,%lf,%lf,%lf,%lf,", job->id,
                             job->submission_time,
                             job->nb_res,
                             job->walltime,
                             success,
                             job->startingTime,
                             job->runtime,
                             job->startingTime + job->runtime, // finish_time
                             job->startingTime - job->submission_time, // waiting_time
                             job->startingTime + job->runtime - job->submission_time, // turnaround_time
                             (job->startingTime + job->runtime - job->submission_time) / job->runtime // stretch
                             );
                    xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                    fputs(buf, f);
                    free(buf);

                    if (job->nb_res > 0)
                    {
                        int ret = asprintf(&buf, "%d", job->alloc_ids[0]);
                        xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                        fputs(buf, f);
                        free(buf);

                        for (int i = 1; i < job->nb_res; ++i)
                        {
                            ret = asprintf(&buf," %d", job->alloc_ids[i]);
                            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                            fputs(buf, f);
                            free(buf);
                        }
                    }

                    fputs("\n", f);
                }
            }
        }

        int err = fclose(f);
        xbt_assert(err == 0, "Impossible to close file '%s'...", filename);
    }
    else
        XBT_INFO("Impossible to write file '%s'", filename);
}*/

void exportScheduleToCSV(const char *filename, double scheduling_time)
{

}

/*
TODO
void exportScheduleToCSV(const char *filename, double scheduling_time)
{
    FILE * f = fopen(filename, "w");

    if (f != NULL)
    {
        fputs("nb_jobs,nb_jobs_finished,nb_jobs_success,nb_jobs_killed,success_rate,makespan,max_turnaround_time,scheduling_time,jobs_execution_time_boundary_ratio\n", f);

        if (jobs_dynar != NULL)
        {
            unsigned int i;
            s_job_t * job;
            char * buf;

            int nb_jobs = 0;
            int nb_jobs_finished = 0;
            int nb_jobs_success = 0;
            int nb_jobs_killed = 0;
            double makespan = 0;
            double max_turnaround_time = 0;
            double min_job_execution_time = DBL_MAX;
            double max_job_execution_time = DBL_MIN;

            xbt_dynar_foreach(jobs_dynar, i, job)
            {
                nb_jobs++;

                if (job->state == JOB_STATE_COMPLETED_SUCCESSFULLY || job->state == JOB_STATE_COMPLETED_KILLED)
                {
                    nb_jobs_finished++;

                    if (job->runtime < min_job_execution_time)
                        min_job_execution_time = job->runtime;
                    if (job->runtime > max_job_execution_time)
                        max_job_execution_time = job->runtime;

                    if (job->state == JOB_STATE_COMPLETED_SUCCESSFULLY)
                        nb_jobs_success++;
                    else
                        nb_jobs_killed++;

                    double completion_time = job->startingTime + job->runtime;
                    double turnaround_time = job->startingTime + job->runtime - job->submission_time;

                    if (completion_time > makespan)
                        makespan = completion_time;

                    if (turnaround_time > max_turnaround_time)
                        max_turnaround_time = turnaround_time;
                }
            }

            int ret = asprintf(&buf, "%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf\n",
                     nb_jobs, nb_jobs_finished, nb_jobs_success, nb_jobs_killed,
                     (double)nb_jobs_success/nb_jobs, makespan, max_turnaround_time,
                     scheduling_time, max_job_execution_time / min_job_execution_time);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

            fputs(buf, f);
            free(buf);

            int err = fclose(f);
            xbt_assert(err == 0, "Impossible to close file '%s'...", filename);
        }
    }
    else
        XBT_INFO("Impossible to write file '%s'", filename);
}
*/

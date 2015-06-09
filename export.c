#include "export.h"

#include <stdlib.h>
#include <xbt.h>
#include <math.h>
#include <float.h>

#include "job.h"
#include "utils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(export, "export");

WriteBuffer * writeBuffer_create(const char * filename, int bufferSize)
{
	xbt_assert(bufferSize > 0, "Invalid buffer size (%d)", bufferSize);

    WriteBuffer * wbuf = xbt_new(WriteBuffer, 1);

	wbuf->f = fopen(filename, "w"); 
	wbuf->bufferSize = bufferSize;
    wbuf->buffer = xbt_new0(char, bufferSize);
	wbuf->bufferPos = 0;

	xbt_assert(wbuf->f != NULL, "Cannot write file '%s'", filename);
	xbt_assert(wbuf->buffer != NULL, "Cannot allocate a buffer of size %d", bufferSize);

	return wbuf;
}

void writeBuffer_destroy(WriteBuffer ** wbuf)
{
	if (*wbuf != NULL)
	{
		if ((*wbuf)->bufferPos > 0)
			writeBuffer_flush(*wbuf);

		int err = fclose((*wbuf)->f);
		free((*wbuf)->buffer);
		free(*wbuf);
		*wbuf = NULL;

		xbt_assert(err == 0, "File closure failed");
	}
}

void writeBuffer_append(WriteBuffer * wbuf, const char * text)
{
	xbt_assert(wbuf != NULL);
	xbt_assert(text != NULL);

	const int textLength = strlen(text);
		
	// If the buffer is big enough
	if (wbuf->bufferPos + textLength < wbuf->bufferSize)
	{
		// Let's append the text to the buffer
		memcpy(wbuf->buffer + wbuf->bufferPos, text, textLength * sizeof(char));
		wbuf->bufferPos += textLength;
	}
	else
	{	
		// Let's write the current buffer content in the file
		writeBuffer_flush(wbuf);

		// Let's write the text in the buffer
		xbt_assert(textLength < wbuf->bufferSize, "Text too large to fit in the buffer (%d required, only got %d)", textLength, wbuf->bufferSize);
		memcpy(wbuf->buffer, text, textLength * sizeof(char));
		wbuf->bufferPos = textLength;
	}
}

void writeBuffer_flush(WriteBuffer * wbuf)
{
	xbt_assert(wbuf != NULL);

	int err = fputs(wbuf->buffer, wbuf->f);
	wbuf->bufferPos = 0;

	xbt_assert(err >= 0, "Write string to file failure");
}



// Since there is no module nor module-like (namespace, class) feature in C,
// Let's define some constants here
/**
 * @brief Enumerates the different Pajé keywords
 */
enum PajeKeyword
{
    PAJE_DEFINE_CONTAINER_TYPE = 1  //!< Defines a container type
    ,PAJE_CREATE_CONTAINER          //!< Creates a container
    ,PAJE_DESTROY_CONTAINER         //!< Destroys a container
    ,PAJE_DEFINE_STATE_TYPE         //!< Defines a state type
    ,PAJE_DEFINE_ENTITY_VALUE       //!< Define an entity value
    ,PAJE_SET_STATE                 //!< Sets a state
    ,PAJE_DEFINE_EVENT_TYPE         //!< Defines an event type
    ,PAJE_NEW_EVENT                 //!< Creates an event
    ,PAJE_DEFINE_VARIABLE_TYPE      //!< Defines a variable type
    ,PAJE_SET_VARIABLE              //!< Set a variable
};

const char * PAJE_rootType = "root_ct";
const char * PAJE_machineType = "machine_ct";
const char * PAJE_machineState = "machine_state";
const char * PAJE_schedulerType = "scheduler_ct";
const char * PAJE_killerType = "killer_ct";
const char * PAJE_killEventKiller = "kk";
const char * PAJE_killEventMachine = "km";
const char * PAJE_utilizationVarType = "vu_vt";
const char * PAJE_mstateWaiting = "w";
const char * PAJE_mstateLaunching = "l";
const char * PAJE_varGlobalUtilization = "vgu";
const char * PAJE_root = "root";
const char * PAJE_scheduler = "sc";
const char * PAJE_killer = "k";
const char * PAJE_machinePrefix = "m";
const char * PAJE_jobPrefix = "j";
const char * PAJE_waitingColor= "\"0.0 0.0 0.0\"";
const char * PAJE_launchingColor = "\"0.3 0.3 0.3\"";
const char * PAJE_utilizationColor = "\"0.0 0.5 0.0\"";


PajeTracer * pajeTracer_create(const char * filename, int logLaunchings, int colorCount)
{
	xbt_assert(logLaunchings == 0 || logLaunchings == 1, "logLaunchings must be either 0 or 1");
	xbt_assert(colorCount > 0);

    PajeTracer * tracer = xbt_new(PajeTracer, 1);
	xbt_assert(tracer != NULL);

	tracer->state = PAJE_STATE_UNINITIALIZED;
	tracer->logLaunchings = logLaunchings;
	tracer->buf = writeBuffer_create(filename, 1024*1024);
	tracer->colorCount = colorCount;
	tracer->colors = NULL;

	pajeTracer_private_generateColors(tracer, colorCount);

	return tracer;
}

void pajeTracer_destroy(PajeTracer ** tracer)
{
	if (tracer != NULL)
	{
		if ((*tracer)->state != PAJE_STATE_FINALIZED)
			fprintf(stderr, "Destruction of a PajeTracer object which was not finalized. The corresponding trace file may be invalid.");

		writeBuffer_destroy(&((*tracer)->buf));

		for (int i = 0; i < (*tracer)->colorCount; ++i)
			free((*tracer)->colors[i]);

		free((*tracer)->colors);
		free(*tracer);
		*tracer = NULL;
	}
}

void pajeTracer_initialize(PajeTracer * tracer, double date, int machineCount, const msg_host_t * machines)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_UNINITIALIZED, "Bad call: the object is not UNINITIALIZED");
	xbt_assert(machineCount > 0);
	xbt_assert(machines != NULL);

	// The buffer used to write lines in the file
	const int bufSize = 1024;
	char buf[bufSize];

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
        "%% Color color\n"
		"%%EndEventDef\n"
		"\n"
		"%%EventDef PajeSetVariable %d\n"
		"%% Time date\n"
		"%% Type string\n"
		"%% Container string\n"
		"%% Value double\n"
		"%%EndEventDef\n"
		"\n",
		PAJE_DEFINE_CONTAINER_TYPE, PAJE_CREATE_CONTAINER, PAJE_DESTROY_CONTAINER,
		PAJE_DEFINE_STATE_TYPE, PAJE_DEFINE_ENTITY_VALUE, PAJE_SET_STATE,
		PAJE_DEFINE_EVENT_TYPE, PAJE_NEW_EVENT, PAJE_DEFINE_VARIABLE_TYPE,
		PAJE_SET_VARIABLE);
	writeBuffer_append(tracer->buf, buf);

	// Let's create our container types
	snprintf(buf, bufSize,
		"# Container types creation\n"
		"%d 0 %s \"Machines\"\n"
		"%d %s %s \"Machine\"\n"
		"%d 0 %s \"Scheduler\"\n"
		"%d %s %s \"Killer\"\n"
		"\n",
		PAJE_DEFINE_CONTAINER_TYPE,                		PAJE_rootType,
		PAJE_DEFINE_CONTAINER_TYPE, PAJE_rootType, 		PAJE_machineType,
		PAJE_DEFINE_CONTAINER_TYPE,                		PAJE_schedulerType,
		PAJE_DEFINE_CONTAINER_TYPE, PAJE_schedulerType, PAJE_killerType);
	writeBuffer_append(tracer->buf, buf);

	// Let's create our event types
	snprintf(buf, bufSize,
		"# Event types creation\n"
		"%d %s %s \"Job kill\"\n"
		"%d %s %s \"Job kill\"\n"
		"\n",
		PAJE_DEFINE_EVENT_TYPE, PAJE_killerType, PAJE_killEventKiller,
		PAJE_DEFINE_EVENT_TYPE, PAJE_machineType, PAJE_killEventMachine);
	writeBuffer_append(tracer->buf, buf);

	// Let's create our variable types
	snprintf(buf, bufSize,
		"# Variable types creation\n"
		"%d %s %s \"Utilization\" %s\n"
		"\n",
		PAJE_DEFINE_VARIABLE_TYPE, PAJE_schedulerType,
		PAJE_utilizationVarType, PAJE_utilizationColor);
	writeBuffer_append(tracer->buf, buf);

	// Let's create our containers
	snprintf(buf, bufSize,
		"# Containers creation\n"
		"%d %lf %s %s \"Machines\" 0\n",
		PAJE_CREATE_CONTAINER, date, PAJE_rootType, PAJE_root);
	writeBuffer_append(tracer->buf, buf);

	for (int i = 0; i < machineCount; ++i)
	{
		const char * machineName = MSG_host_get_name(machines[i]);
		// todo: clean machine name

		snprintf(buf, bufSize,
			"%d %lf %s %s%d \"%s\" %s\n",
			PAJE_CREATE_CONTAINER, date, PAJE_machineType,
			PAJE_machinePrefix, i,
			machineName, PAJE_root);
		writeBuffer_append(tracer->buf, buf);
	}

	snprintf(buf, bufSize,
		"%d %lf %s %s \"Scheduler\" 0\n"
		"%d %lf %s %s \"Killer\" %s\n"
		"\n",
		PAJE_CREATE_CONTAINER, date, PAJE_schedulerType, PAJE_scheduler,
		PAJE_CREATE_CONTAINER, date, PAJE_killerType, PAJE_killer, PAJE_scheduler);
	writeBuffer_append(tracer->buf, buf);

	// Let's declare that machines have a state
	snprintf(buf, bufSize,
		"# States creation\n"
		"%d %s %s \"Machine state\"\n"
		"\n",
		PAJE_DEFINE_STATE_TYPE, PAJE_machineState, PAJE_machineType);
	writeBuffer_append(tracer->buf, buf);

	// Let's declare some machine states
	snprintf(buf, bufSize,
		"# Creation of the different values the machine state can be\n"
		"%d %s %s \"Waiting\" %s\n"
		"%d %s %s \"Launching\" %s\n"
		"\n"
		"# Begin of events\n",
		PAJE_DEFINE_ENTITY_VALUE, PAJE_mstateWaiting, PAJE_machineState, PAJE_waitingColor,
		PAJE_DEFINE_ENTITY_VALUE, PAJE_mstateLaunching, PAJE_machineState, PAJE_launchingColor);
	writeBuffer_append(tracer->buf, buf);

	for (int i = 0; i < machineCount; ++i)
	{
		snprintf(buf, bufSize,
			"%d %lf %s %s%d %s\n",
			PAJE_SET_STATE, date, PAJE_machineState, PAJE_machinePrefix, i, PAJE_mstateWaiting);
		writeBuffer_append(tracer->buf, buf);
	}

	tracer->state = PAJE_STATE_INITIALIZED;
}

void pajeTracer_finalize(PajeTracer * tracer, double date, int machineCount, const msg_host_t * machines)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_INITIALIZED, "Bad call: the object is not INITIALIZED");
	(void) machines;

	const int bufSize = 64;
	char buf[bufSize];

	snprintf(buf, bufSize,
		"\n"
		"# End of events, containers destruction\n");
	writeBuffer_append(tracer->buf, buf);

	for (int i = 0; i < machineCount; ++i)
	{
		snprintf(buf, bufSize,
			"%d %lf %s%d %s\n",
			PAJE_DESTROY_CONTAINER, date, PAJE_machinePrefix, i, PAJE_machineType);
		writeBuffer_append(tracer->buf, buf);
	}

	snprintf(buf, bufSize,
		"%d %lf %s %s\n",
			PAJE_DESTROY_CONTAINER, date, PAJE_root, PAJE_rootType);
	writeBuffer_append(tracer->buf, buf);

	tracer->state = PAJE_STATE_FINALIZED;
}

void pajeTracer_private_generateColors(PajeTracer * tracer, int colorCount)
{
	xbt_assert(tracer != NULL);
	xbt_assert(colorCount > 0);

	double h, s=1, v=1, r, g, b;
	const int bufSize = 32;

    tracer->colors = xbt_new(char*, colorCount);
	xbt_assert(tracer->colors != NULL);

	double hueFraction = 360.0 / colorCount;
	for (int i = 0; i < colorCount; ++i)
	{
        tracer->colors[i] = xbt_new(char, bufSize);
		xbt_assert(tracer->colors[i] != NULL);

		h = i * hueFraction;
		hsvToRgb(h,s,v, &r,&g,&b);

		snprintf(tracer->colors[i], bufSize, "\"%lf %lf %lf\"", r, g, b);
	}
}

void pajeTracer_addJobLaunching(PajeTracer * tracer, double date, int jobID,
								int usedMachinesCount, const int * usedMachineIDs)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_INITIALIZED, "Bad call: the PajeTracer object is not initialized or had been finalized");

	const int bufSize = 64;
	char buf[bufSize];

	// Let's create a state value corresponding to this job
	snprintf(buf, bufSize,
		"%d %s%d %s \"%d\" %s\n",
		PAJE_DEFINE_ENTITY_VALUE, PAJE_jobPrefix, jobID, PAJE_machineState, jobID, 
		tracer->colors[jobID % tracer->colorCount]);
	writeBuffer_append(tracer->buf, buf);

	if (tracer->logLaunchings)
	{
		// Let's change the state of all the machines which launch the job
		for (int i = 0; i < usedMachinesCount; ++i)
		{
			snprintf(buf, bufSize,
				"%d %lf %s %s%d %s\n",
				PAJE_SET_STATE, date, PAJE_machineState, PAJE_machinePrefix,
				usedMachineIDs[i], PAJE_mstateLaunching);
			writeBuffer_append(tracer->buf, buf);
		}
	}
}

void pajeTracer_addJobRunning(PajeTracer * tracer, double date, int jobID,
							  int usedMachinesCount, const int * usedMachineIDs)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_INITIALIZED, "Bad call: the PajeTracer object is not initialized or had been finalized");

	const int bufSize = 64;
	char buf[bufSize];

	// Let's change the state of all the machines which run the job
	for (int i = 0; i < usedMachinesCount; ++i)
	{
		snprintf(buf, bufSize,
			"%d %lf %s %s%d %s%d\n",
			PAJE_SET_STATE, date, PAJE_machineState, PAJE_machinePrefix,
			usedMachineIDs[i], PAJE_jobPrefix, jobID);
		writeBuffer_append(tracer->buf, buf);
	}
}

void pajeTracer_addJobEnding(PajeTracer * tracer, double date, int jobID,
							 int usedMachinesCount, const int * usedMachineIDs)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_INITIALIZED, "Bad call: the PajeTracer object is not initialized or had been finalized");
	(void) jobID;

	const int bufSize = 64;
	char buf[bufSize];

	// Let's change the state of all the machines which run the job
	for (int i = 0; i < usedMachinesCount; ++i)
	{
		snprintf(buf, bufSize,
			"%d %lf %s %s%d %s\n",
			PAJE_SET_STATE, date, PAJE_machineState,
			PAJE_machinePrefix, usedMachineIDs[i], PAJE_mstateWaiting);
		writeBuffer_append(tracer->buf, buf);
	}
}

void pajeTracer_addJobKill(PajeTracer * tracer, double date, int jobID,
						   int usedMachinesCount, const int * usedMachineIDs)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_INITIALIZED, "Bad call: the PajeTracer object is not initialized or had been finalized");

	const int bufSize = 64;
	char buf[bufSize];

	// Let's add a kill event associated with the scheduler
	snprintf(buf, bufSize,
		"%d %lf %s %s \"%d\"\n",
		PAJE_NEW_EVENT, date, PAJE_killEventKiller, PAJE_killer, jobID);
	writeBuffer_append(tracer->buf, buf);

	// Let's add a kill event associated with each machine
	for (int i = 0; i < usedMachinesCount; ++i)
	{
		snprintf(buf, bufSize,
			"%d %lf %s %s%d \"%d\"\n",
			PAJE_NEW_EVENT, date, PAJE_killEventMachine,
			PAJE_machinePrefix, usedMachineIDs[i], jobID);
		writeBuffer_append(tracer->buf, buf);
	}
}

void pajeTracer_addGlobalUtilization(PajeTracer * tracer, double date, double utilization)
{
	xbt_assert(tracer != NULL);
	xbt_assert(tracer->state == PAJE_STATE_INITIALIZED, "Bad call: the PajeTracer object is not initialized or had been finalized");

	const int bufSize = 64;
	char buf[bufSize];

	// Let's set the variable state correctly
	snprintf(buf, bufSize,
		"%d %lf %s %s %lf\n",
		PAJE_SET_VARIABLE, date, PAJE_utilizationVarType, PAJE_scheduler, utilization);
	writeBuffer_append(tracer->buf, buf);
}

void pajeTracer_private_shuffleColors(PajeTracer * tracer)
{
	xbt_assert(tracer != NULL);

	if (tracer->colorCount > 1)
	{
		for (int i = 0; i < tracer->colorCount - 1; i++)
		{
			size_t j = i + rand() / (RAND_MAX / (tracer->colorCount - i) + 1);
			char * t = tracer->colors[j];
			tracer->colors[j] = tracer->colors[i];
			tracer->colors[i] = t;
		}
	}
}

void hsvToRgb(double h, double s, double v, double * r, double * g, double * b)
{
	if (s == 0) // Achromatic (grey)
	{
		*r = *g = *b = v;
		return;
	}

	h /= 60;			// sector 0 to 5
	int i = floor(h);
	float f = h-i;		// factorial part of h
	float p = v*(1-s);
	float q = v*(1-s*f);
	float t = v*(1-s*(1-f));

	switch(i)
	{
	case 0:
		*r = v;
		*g = t;
		*b = p;
		break;
	case 1:
		*r = q;
		*g = v;
		*b = p;
		break;
	case 2:
		*r = p;
		*g = v;
		*b = t;
		break;
	case 3:
		*r = p;
		*g = q;
		*b = v;
		break;
	case 4:
		*r = t;
		*g = p;
		*b = v;
		break;
	default:	// case 5:
		*r = v;
		*g = p;
		*b = q;
		break;
	}
}


void exportJobsToCSV(const char *filename)
{
    FILE * f = fopen(filename, "w");

    /*jobID
    submit time
    number of processors
    rejected (0 or 1)
    wall time
    starting time
    execution time
    used processors (list of ranges)*/

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
}


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

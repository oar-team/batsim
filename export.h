#pragma once

#include <stdio.h>
#include <sys/types.h> /* ssize_t, needed by xbt/str.h, included by msg/msg.h */
#include <simgrid/msg.h>

/**
 * @brief Handles bufferized writings into a single file
 * @details Instead of applying writings directly to the file, a memory buffer is used. When the buffer is full, it is flushed to the disk.
 */
typedef struct
{
	FILE * f; 		//! The file stream
	int bufferSize; //! The size of the buffer (octets)
	char * buffer;  //! The buffer
	int bufferPos;  //! The number of octets currently in the buffer
} WriteBuffer;

/**
 * @brief Creates a WriteBuffer
 * @param[in] filename The file name
 * @param[in] bufferSize The strictly positive buffer size
 * 
 * @return The newly created WriteBuffer
 */
WriteBuffer * writeBuffer_create(const char * filename, int bufferSize);

/**
 * @brief Destroys a WriteBuffer
 * @param[in, out] wbuf The WriteBuffer
 */
void writeBuffer_destroy(WriteBuffer ** wbuf);

/**
 * @brief Appends a text at the end of the buffer. If the buffer is full, it is automatically flushed into the disk.
 * @param wbuf The WriteBuffer
 * @param text The text to push in the buffer
 */
void writeBuffer_append(WriteBuffer * wbuf, const char * text);

/**
 * @brief Flushes the current content of the buffer into the disk.
 * @param wbuf The WriteBuffer
 */
void writeBuffer_flush(WriteBuffer * wbuf);

/**
 * @brief Exports the job execution to a CSV file.
 * @param filename This is the name of the output file used to write the CSV data.
 */
void exportJobsToCSV(const char * filename);

/**
 * @brief The states of a PajeTracer
 */
typedef enum
{
	PAJE_STATE_UNINITIALIZED, //! The PajeTracer has not been initialized yet
	PAJE_STATE_INITIALIZED,   //! The PajeTracer has been initialized
	PAJE_STATE_FINALIZED      //! The PajeTracer has been finalized
} PajeTracerState;

/**
 * @brief Allows to handle a Pajé trace corresponding to a schedule
 */
typedef struct
{
	PajeTracerState state;	//! The current state of the PajeTracer
	int logLaunchings; 		//! If set to TRUE, job launchings will appear in the Pajé file
	WriteBuffer * buf; 		//! Handles the writings to the file
	int colorCount; 		//! The number of different colors used for the jobs
	char ** colors; 		//! The different colors used for the jobs (as C strings)
} PajeTracer;

/**
 * @brief Creates a PajeTracer object
 * @param[in] The name of the output file in which the Pajé trace is stored
 * @param[in] If set to true, job launching time will be written in the trace. This option leads to larger trace files.
 * @param[in] colorCount The number of different colors to use for jobs
 * @return The newly created PajeTracer object
 */
PajeTracer * pajeTracer_create(const char * filename, int logLaunchings, int colorCount);

/**
 * @brief Destroys a PajeTracer object
 * @param[in,out] tracer The PajeTracer object
 */
void pajeTracer_destroy(PajeTracer ** tracer);

/**
 * @brief Initializes a PajeTracer object.
 * @details This function must be called once before adding job launchings, runnings or endings.
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] machineCount The number of machines
 * @param[in] machines The machines
 */
void pajeTracer_initialize(PajeTracer * tracer, double date, int machineCount, const msg_host_t * machines);

/**
 * @brief Finalizes a PajeTracer object.
 * @details This function must be called before the object destruction.
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] machineCount The number of machines
 * @param[in] machines The machines
 */
void pajeTracer_finalize(PajeTracer * tracer, double date, int machineCount, const msg_host_t * machines);

/**
 * @brief Adds a job launching in the Pajé trace.
 * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet.
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] jobID The job number
 * @param[in] usedMachinesCount The number of used machines to run the job
 * @param[in] usedMachineIDs The numbers of the machines used to run the job
 */
void pajeTracer_addJobLaunching(PajeTracer * tracer, double date, int jobID, int usedMachinesCount, const int * usedMachineIDs);
/**
 * @brief Adds a job running in the Pajé trace.
 * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet. Furthermore, the corresponding job launching must had been added previously.
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] jobID The job number
 * @param[in] usedMachinesCount The number of used machines to run the job
 * @param[in] usedMachineIDs The numbers of the machines used to run the job
 */
void pajeTracer_addJobRunning(PajeTracer * tracer, double date, int jobID, int usedMachinesCount, const int * usedMachineIDs);
/**
 * @brief Adds a job ending in the Pajé trace.
 * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet. Furthermore, the corresponding job launching and running must had been added previously.
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] jobID The job number
 * @param[in] usedMachinesCount The number of used machines to run the job
 * @param[in] usedMachineIDs The numbers of the machines used to run the job
 */
void pajeTracer_addJobEnding(PajeTracer * tracer, double date, int jobID, int usedMachinesCount, const int * usedMachineIDs);
/**
 * @brief Adds a job kill in the Pajé trace.
 * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet. Furthermore, the corresponding job launching must had been added previously.
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] jobID The job number
 * @param[in] usedMachinesCount The number of used machines to run the job
 * @param[in] usedMachineIDs The numbers of the machines used to run the job
 */
void pajeTracer_addJobKill(PajeTracer * tracer, double date, int jobID, int usedMachinesCount, const int * usedMachineIDs);
/**
 * @brief Adds the system global utilization in the Pajé trace.
 * @details Please note that this method can only be called when the PajeTracer object has been initialized and had not been finalized yet
 * @param[in,out] tracer The PajeTracer object
 * @param[in] date The current simulation date
 * @param[in] utilization The system global utilization.
 */
void pajeTracer_addGlobalUtilization(PajeTracer * tracer, double date, double utilization);

/**
 * @brief Generate colors
 * @details The colors are fairly shared in the Hue color spectrum.
 * @param[in,out] tracer The PajeTracer
 * @param[in] colorCount colorCount The number of colors to generate
 */
void pajeTracer_private_generateColors(PajeTracer * tracer, int colorCount);

/**
 * @brief Randomize the position of the colors in the colormap
 * @param[in,out] tracer The PajeTracer
 */
void pajeTracer_private_shuffleColors(PajeTracer * tracer);

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
void hsvToRgb(double h, double s, double v, double * r, double * g, double * b);

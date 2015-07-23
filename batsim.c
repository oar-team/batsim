/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "batsim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <stdbool.h>

#include <simgrid/msg.h>

#include <xbt/sysdep.h>         /* calloc, printf */
/* Create a log channel to have nice outputs. */
#include <xbt/log.h>
#include <xbt/asserts.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "Batsim");
XBT_LOG_NEW_CATEGORY(network, "Network");

#include "job.h"
#include "utils.h"
#include "export.h"
#include "machines.h"

//! The total number of microseconds used by the external scheduler
long long microseconds_used_by_scheduler = 0;
//! The maximum length of the messages sent to the external scheduler
const int schedMessageMaxLength = 1024*1024 - 1; // 1 Mio should be enough...

//! Associates names to struct e_task_type_t
char *task_type2str[] =
{
    "FINALIZE",
    "LAUNCH_JOB",
    "JOB_SUBMITTED",
    "JOB_COMPLETED",
    "SCHED_EVENT",
    "SCHED_READY",
    "LAUNCHER_INFORMATION",
    "KILLER_INFORMATION",
    "SUBMITTER_HELLO",
    "SUBMITTER_BYE"
};

/**
 * @brief Compares two machines in function of their name (lexicographic order)
 * @param[in] e1 The first machine
 * @param[in] e2 The second machine
 * @return An integer less than, equal to, or greater than zero if e1 is found, respectively, to be less than, to match, or be greater than e2
 */
static int machine_comparator(const void * e1, const void * e2)
{
    const msg_host_t * m1 = (msg_host_t *) e1;
    const msg_host_t * m2 = (msg_host_t *) e2;

    return strcmp(MSG_host_get_name(*m1), MSG_host_get_name(*m2));
}

//! The number of computing nodes
int nb_nodes = 0;
//! The computing nodes
msg_host_t *nodes;
//! The Unix Domain Socket file descriptor
int uds_fd = -1;
//! The PajeTracer object
PajeTracer * tracer = NULL;

// process functions
static int node(int argc, char *argv[]);
static int server(int argc, char *argv[]);
static int launch_job(int argc, char *argv[]);
static void send_message(const char *dst, e_task_type_t type, int job_id, void *data);

/**
 * @brief Sends a message on the Unix Domain Socket
 * @param[in] msg The message to send
 * @return 0
 */
static int send_uds(char * msg)
{
    int32_t lg = strlen(msg);
    XBT_CINFO(network, "Sending '%s'", msg);
    write(uds_fd, &lg, 4);
    write(uds_fd, msg, lg);

    return 0;
}

/**
 * @brief Receives a message from the Unix Domain Socket
 * @return The message (must be freed by the caller)
 */
static char *recv_uds()
{
    int32_t lg;
    char *msg;
    read(uds_fd, &lg, 4);
    //XBT_INFO("Received message length: %d bytes", lg);
    xbt_assert(lg > 0, "Invalid message received (size=%d)", lg);
    msg = xbt_new(char, lg+1); /* +1 for null terminator */
    read(uds_fd, msg, lg);
    msg[lg] = 0;
    XBT_CINFO(network, "Received '%s'", msg);

    return msg;
}

/**
 * @brief Opens the Unix Domain Socket to communicate with the external scheduler
 * @param[in] socketFilename The socket filename
 * @param[in] nb_try The maximum number of connection attempts
 * @param[in] msBetweenTries The delay (in milliseconds) between two connection tries
 */
static void open_uds(const char * socketFilename, int nb_try, double msBetweenTries)
{
    struct sockaddr_un address;

    uds_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(uds_fd < 0)
    {
        XBT_CERROR(network, "socket() failed\n");
        exit(1);
    }
    /* start with a clean address structure */
    memset(&address, 0, sizeof(struct sockaddr_un));

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, 255, "%s", socketFilename);

    int connected = 0;
    for (int i = 0; i < nb_try && !connected; ++i)
    {
        XBT_CINFO(network, "Trying to connect to '%s' (try %d/%d)", socketFilename, i+1, nb_try);

        if(connect(uds_fd,
                   (struct sockaddr *) &address,
                   sizeof(struct sockaddr_un)) != 0)
        {
            if (i < nb_try-1)
            {
                XBT_CINFO(network, "Failed... Trying again in %g ms\n", msBetweenTries);
                usleep(msBetweenTries * 1000);
            }
            else
                XBT_CINFO(network, "Failed...");
        }
        else
            connected = 1;
    }

    if (!connected)
        xbt_die("Connection failed %d times, aborting.", nb_try);
}

/**
 * @brief Sends a request to the scheduler (via UDS) and handles its reply.
 * @details This function sends a SCHED_EVENT by event received then a SCHED_READY when all events have been sent
 * @return 1
 */
static int requestReplyScheduler(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    struct timeval t0;
    struct timeval t1;

    char sendDateAsString[16];
    sprintf(sendDateAsString, "%f", MSG_get_clock());

    char *sendBuf = MSG_process_get_data(MSG_process_self());
    XBT_INFO("Buffer received in REQ-REP: '%s'", sendBuf);

    send_uds(sendBuf);
    free(sendBuf);

    gettimeofday(&t0, 0);
    char * message_recv = recv_uds();

    gettimeofday(&t1, 0);
    long long elapsed = (t1.tv_sec-t0.tv_sec)*1000000LL + t1.tv_usec-t0.tv_usec;
    microseconds_used_by_scheduler += elapsed;

    xbt_dynar_t answer_dynar = xbt_str_split(message_recv, "|");

    int answerParts = xbt_dynar_length(answer_dynar);
    xbt_assert(answerParts >= 2, "Invalid message received ('%s'): it should be composed of at least 2 parts separated by a '|'", message_recv);

    double previousDate = atof(sendDateAsString);

    for (int i = 1; i < answerParts; ++i)
    {
        char * eventString = *(char **) xbt_dynar_get_ptr(answer_dynar, i);
        xbt_dynar_t * eventDynar = xbt_new(xbt_dynar_t, 1);
        *eventDynar = xbt_str_split(eventString, ":");
        int eventParts = xbt_dynar_length(*eventDynar);
        xbt_assert(eventParts >= 2, "Invalid event received ('%s'): it should be composed of at least 2 parts separated by a ':'", eventString);

        double eventDate = atof(*(char **)xbt_dynar_get_ptr(*eventDynar, 0));
        xbt_assert(eventDate >= previousDate, "Invalid event received ('%s'): its date (%lf) cannot be before the previous event date (%lf)",
                   eventString, eventDate, previousDate);

        MSG_process_sleep(max(0, eventDate - previousDate));

        send_message("server", SCHED_EVENT, -1, eventDynar);

        previousDate = eventDate;
    }

    send_message("server", SCHED_READY, 0, NULL);

    xbt_dynar_free(&answer_dynar);
    free(message_recv);
    return 0;
}

/**
 * @brief Sends a message from the given process to the given mailbox
 * @param[in] dst The destination mailbox
 * @param[in] type The type of message to send
 * @param[in] job_id The job the message is about
 * @param[in] data The data associated to the message
 */
void send_message(const char *dst, e_task_type_t type, int job_id, void *data)
{
    msg_task_t task_sent;
    s_task_data_t * req_data = xbt_new(s_task_data_t, 1);
    req_data->type = type;
    req_data->job_id = job_id;
    req_data->data = data;

    task_sent = MSG_task_create(NULL, 0, 1e-6, req_data);

    XBT_INFO("message from '%s' to '%s' of type '%s' with data %p",
             MSG_process_get_name(MSG_process_self()), dst, task_type2str[type], data);

    MSG_task_send(task_sent, dst);
}

/**
 * @brief Frees a task then makes it point to NULL
 * @param[in, out] The task to free
 */
static void task_free(msg_task_t * task)
{
    if(*task != NULL)
    {
        MSG_task_destroy(*task);
        *task = NULL;
    }
}

/**
 * @brief The function in charge of job launching
 * @param[in] argc The number of input arguments
 * @param[in] argv The input arguments
 * @return 0
 */
static int launch_job(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    s_launch_data_t * data = MSG_process_get_data(MSG_process_self());
    int jobID = data->jobID;
    s_job_t * job = jobFromJobID(jobID);

    XBT_INFO("Launching job %d", jobID);
    job->startingTime = MSG_get_clock();
    job->alloc_ids = data->reservedNodesIDs;
    pajeTracer_addJobLaunching(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);
    pajeTracer_addJobRunning(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);

    updateMachinesOnJobRun(jobID, data->reservedNodeCount, data->reservedNodesIDs);

    if (job_exec(jobID, data->reservedNodeCount, data->reservedNodesIDs, nodes, job->walltime) == 1)
    {
        XBT_INFO("Job %d finished in time", data->jobID);
        job->state = JOB_STATE_COMPLETED_SUCCESSFULLY;
        pajeTracer_addJobEnding(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);
    }
    else
    {
        XBT_INFO("Job %d had been killed (walltime %lf reached)", job->id, job->walltime);
        job->state = JOB_STATE_COMPLETED_KILLED;
        pajeTracer_addJobEnding(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);
        pajeTracer_addJobKill(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);
    }

    job->runtime = MSG_get_clock() - job->startingTime;

    updateMachinesOnJobEnd(jobID, data->reservedNodeCount, data->reservedNodesIDs);

    //free(data->reservedNodesIDs);
    free(data);
    send_message("server", JOB_COMPLETED, jobID, NULL);

    return 0;
}

/**
 * @brief The function executed on each computing node
 * @param[in] argc The number of input arguments
 * @param[in] argv The arguments
 * @return 0
 */
static int node(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    const char *node_id = MSG_host_get_name(MSG_host_self());

    msg_task_t task_received = NULL;
    s_task_data_t * task_data;
    int type = -1;

    XBT_INFO("I am %s", node_id);

    while(1)
    {
        MSG_task_receive(&(task_received), node_id);

        task_data = (s_task_data_t *) MSG_task_get_data(task_received);
        type = task_data->type;

        XBT_INFO("MSG_Task received %s, type %s", node_id, task_type2str[type]);

        switch (type)
        {
            case FINALIZE:
            {
                task_free(&task_received);
                free(task_data);
                return 0;
                break;
            }
            case LAUNCH_JOB:
            {
                // Let's retrieve the launch data and create a kill data
                s_launch_data_t * launchData = (s_launch_data_t *) task_data->data;

                char * launcherName = 0;
                int ret = asprintf(&launcherName, "launcher %d", launchData->jobID);
                xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                MSG_process_create(launcherName, launch_job, launchData, MSG_host_self());
                free(launcherName);

                task_free(&task_received);
                free(task_data);
                break;
            }
            default:
            {
                XBT_ERROR("Unhandled message type received (%d)", type);
            }
        }
    }

    return 0;
}

/**
 * \brief Unroll jobs submission
 *
 * Jobs' submission is managed by a dedicate MSG_process
 */
static int jobs_submitter(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    s_job_t * job;
    unsigned int job_index;

    send_message("server", SUBMITTER_HELLO, 0, NULL);

    double previousSubmissionDate = MSG_get_clock();

    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {
        if (job->submission_time > previousSubmissionDate)
            MSG_process_sleep(job->submission_time - previousSubmissionDate);

        send_message("server", JOB_SUBMITTED, job->id, NULL);
        previousSubmissionDate = MSG_get_clock();
    }

    send_message("server", SUBMITTER_BYE, 0, NULL);

    return 0;
}


/**
 * \brief The central process which manage the global orchestration
 */
int server(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    msg_task_t task_received = NULL;
    s_task_data_t * task_data;

    int nb_completed_jobs = 0;
    int nb_submitted_jobs = 0;
    int nb_scheduled_jobs = 0;
    int nb_submitters = 0;
    int nb_submitters_finished = 0;
    int nb_running_jobs = 0;
    int sched_ready = 1;
    int size_m;
    char *tmp;
    char *job_ready_str;
    char *res_str;
    char *job_id_str;
    unsigned int i, j;
    int k;

    xbt_assert(schedMessageMaxLength > 0);
    char *sched_message = xbt_new(char, schedMessageMaxLength+1); // + 1 for NULL-terminated
    xbt_assert(sched_message != NULL, "Cannot allocate the send message buffer (requested bytes: %d)", schedMessageMaxLength+1);
    sched_message[0] = '\0'; // Empties the string
    int lg_sched_message = 0;

    // it may avoid the SG deadlock...
    while ((nb_submitters == 0) || (nb_submitters_finished < nb_submitters) ||
           (nb_completed_jobs < nb_submitted_jobs) || !sched_ready)
    {
        // wait message node, submitter, scheduler...
        MSG_task_receive(&(task_received), "server");
        task_data = (s_task_data_t *) MSG_task_get_data(task_received);

        XBT_INFO("Server receive Task/message type %s:", task_type2str[task_data->type]);

        switch (task_data->type)
        {
        case SUBMITTER_HELLO:
        {
            nb_submitters++;
            XBT_INFO("New submitter said hello. Number of polite submitters: %d", nb_submitters);
            break;
        } // end of case SUBMITTER_HELLO
        case SUBMITTER_BYE:
        {
            nb_submitters_finished++;
            XBT_INFO("A submitted said goodbye. Number of finished submitters: %d", nb_submitters_finished);
            break;
        } // end of case SUBMITTER_BYE
        case JOB_COMPLETED:
        {
            nb_running_jobs--;
            xbt_assert(nb_running_jobs >= 0);
            nb_completed_jobs++;
            s_job_t * job = jobFromJobID(task_data->job_id);

            XBT_INFO("Job %d COMPLETED. %d jobs completed so far", job->id, nb_completed_jobs);
            size_m = snprintf(sched_message + lg_sched_message, schedMessageMaxLength - lg_sched_message,
                              "|%f:C:%s", MSG_get_clock(), job->id_str);
            lg_sched_message += size_m;
            xbt_assert(lg_sched_message <= schedMessageMaxLength,
                       "Buffer for sending messages to the scheduler is not big enough...");
            XBT_INFO("Message to send to scheduler: %s", sched_message);

            break;
        } // end of case JOB_COMPLETED
        case JOB_SUBMITTED:
        {
            nb_submitted_jobs++;
            s_job_t * job = jobFromJobID(task_data->job_id);
            job->state = JOB_STATE_SUBMITTED;

            XBT_INFO("Job %d SUBMITTED. %d jobs submitted so far", job->id, nb_submitted_jobs);
            size_m = snprintf(sched_message + lg_sched_message, schedMessageMaxLength - lg_sched_message,
                              "|%f:S:%s", MSG_get_clock(), job->id_str);
            lg_sched_message += size_m;
            xbt_assert(lg_sched_message <= schedMessageMaxLength,
                        "Buffer for sending messages to the scheduler is not big enough...");
            XBT_INFO("Message to send to scheduler: %s", sched_message);

            break;
        } // end of case JOB_SUBMITTED
        case SCHED_EVENT:
        {
            xbt_dynar_t * input = (xbt_dynar_t *)task_data->data;

            tmp = *(char **)xbt_dynar_get_ptr(*input, 1);

            XBT_INFO("type of receive message from scheduler: %c", *tmp);
            switch (*tmp)
            {
            case 'J': // "timestampJ"
            {
                tmp = *(char **)xbt_dynar_get_ptr(*input, 2); // to skip the timestamp and the 'J'
                xbt_dynar_t jobs_ready_dynar = xbt_str_split(tmp, ";");

                xbt_dynar_foreach(jobs_ready_dynar, i, job_ready_str)
                {
                    if (job_ready_str != NULL)
                    {
                        XBT_INFO("job_ready: %s", job_ready_str);
                        xbt_dynar_t job_id_res = xbt_str_split(job_ready_str, "=");
                        job_id_str = *(char **)xbt_dynar_get_ptr(job_id_res, 0);
                        char * job_reservs_str = *(char **)xbt_dynar_get_ptr(job_id_res, 1);

                        int jobID = strtol(job_id_str, NULL, 10);
                        xbt_assert(jobExists(jobID), "Invalid jobID '%s' received from the scheduler: the job does not exist", job_id_str);
                        s_job_t * job = jobFromJobID(jobID);
                        xbt_assert(job->state == JOB_STATE_SUBMITTED, "Invalid allocation from the scheduler: the job %d is either not submitted yet"
                                   " or already scheduled (state=%d)", jobID, job->state);

                        job->state = JOB_STATE_RUNNING;

                        nb_running_jobs++;
                        xbt_assert(nb_running_jobs <= nb_submitted_jobs);
                        nb_scheduled_jobs++;
                        xbt_assert(nb_scheduled_jobs <= nb_submitted_jobs);

                        xbt_dynar_t res_dynar = xbt_str_split(job_reservs_str, ",");
                        xbt_assert(xbt_dynar_length(res_dynar) > 0, "Invalid allocation from the scheduler: the job %d is scheduled on no host", jobID);

                        int head_node = atoi(*(char **)xbt_dynar_get_ptr(res_dynar, 0));
                        XBT_INFO("head node: %s, id: %d", MSG_host_get_name(nodes[head_node]), head_node);

                        // Let's create a launch data structure, which will be given to the head_node then used to launch the job
                        s_launch_data_t * launchData = xbt_new(s_launch_data_t, 1);
                        launchData->jobID = jobID;
                        launchData->reservedNodeCount = xbt_dynar_length(res_dynar);

                        xbt_assert(launchData->reservedNodeCount == job->nb_res,
                                   "Invalid scheduling algorithm decision: allocation of job %d is done on %d nodes (instead of %d)",
                                   launchData->jobID, launchData->reservedNodeCount, job->nb_res);

                        launchData->reservedNodesIDs = xbt_new(int, launchData->reservedNodeCount);

                        // Let's fill the reserved node IDs from the XBT dynar
                        xbt_dynar_foreach(res_dynar, j, res_str)
                        {
                            int machineID = atoi(res_str);
                            launchData->reservedNodesIDs[j] = machineID;
                            xbt_assert(machineID >= 0 && machineID < nb_nodes,
                                       "Invalid machineID (%d) received from the scheduler: not in range [0;%d[", machineID, nb_nodes);
                        }

                        send_message(MSG_host_get_name(nodes[head_node]), LAUNCH_JOB, jobID, launchData);

                        xbt_dynar_free(&res_dynar);
                        xbt_dynar_free(&job_id_res);
                    } // end of if
                } // end of xbt_dynar_foreach

                xbt_dynar_free(&jobs_ready_dynar);
                break;
            } // end of case J
            case 'N':
            {
                XBT_INFO("Nothing to do received.");
                if (nb_running_jobs == 0 && nb_scheduled_jobs < nb_submitted_jobs)
                {
                    XBT_INFO("Nothing to do whereas no job is running and that they are jobs waiting to be scheduled... This might cause a deadlock!");

                    // Let's display the available jobs (to help the scheduler debugging)
                    int bufSize = 1024;
                    int strSize = 0;
                    char * buf = xbt_new(char, bufSize+1);
                    xbt_assert(sizeof(char) == 1);
                    buf[0] = '\0'; // Empties the string

                    s_job_t * job;
                    unsigned int job_index;

                    xbt_dynar_foreach(jobs_dynar, job_index, job)
                    {
                        if (job->state == JOB_STATE_SUBMITTED)
                        {
                            char * tmp;
                            int ret = asprintf(&tmp, ",%d", job->id);
                            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

                            strncat(buf, tmp, max(0,bufSize - strSize));
                            strSize += strlen(tmp);

                            free(tmp);
                        }
                    }

                    XBT_INFO("The available jobs are:%s", buf);
                    free(buf);
                }
                break;
            } // end of case N
            default:
            {
                XBT_ERROR("Command %s is not supported", tmp);
                break;
            } // end of default
            } // end of switch (inner)

            //XBT_INFO("before freeing dynar");
            xbt_dynar_free(input);
            free(input);
            //XBT_INFO("after freeing dynar");

            break;
        } // end of case SCHED_EVENT
        case SCHED_READY:
        {
            sched_ready = 1;
            break;
        } // end of case SCHED_READY
        default:
        {
            XBT_ERROR("Unhandled data type received (%d)", task_data->type);
        }
        } // end of switch (outer)

        task_free(&task_received);
        free(task_data);

        if (sched_ready && (strcmp(sched_message, "") != 0))
        {
            char * sendBuf;

            int ret = asprintf(&sendBuf, "0:%f%s", MSG_get_clock(), sched_message);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
            sched_message[0] = '\0'; // empties the string
            lg_sched_message = 0;

            MSG_process_create("Sched REQ-REP", requestReplyScheduler, sendBuf, MSG_host_self());

            sched_ready = 0;
        }

    } // end of while

    // send finalize to node
    XBT_INFO("All jobs completed, time to finalize");

    for(k = 0; k < nb_nodes; k++)
        send_message(MSG_host_get_name(nodes[k]), FINALIZE, 0, NULL);

    free(sched_message);

    return 0;
}

/**
 * @brief Deploys the simulator
 * @param[in] platform_file The SimGrid platform filename used in the simulation
 * @param[in] masterHostName The name of the host which will be used for scheduling and not for computing jobs
 * @param[in] pajeTraceFilename The name of the Pajé trace to generate
 * @param[in] csvJobsFilename The name of the CSV output file about jobs
 * @param[in] csvScheduleFilename The name of the CSV output file about the schedule
 * @param[in] smpi_used The flag to indicate the presence of SMPI job
 * @return The msg_error_t result of the inner call of MSG_main() (MSG_OK on success)
 */
msg_error_t deploy_all(const char *platform_file, const char * masterHostName, const char * pajeTraceFilename, const char * csvJobsFilename, const char * csvScheduleFilename, bool smpi_used)
{
    if (!smpi_used)
        MSG_config("host/model", "ptask_L07");
    
    MSG_create_environment(platform_file);

    xbt_dynar_t all_hosts = MSG_hosts_as_dynar();

    // Let's find the master host (the one on which the simulator and the job submitters run)
    msg_host_t master_host = MSG_get_host_by_name(masterHostName);
    int masterIndex = -1;
    xbt_assert(master_host != NULL,"Invalid SimGrid platform file '%s': cannot find any host named '%s'. "
        "This special host is the one on which the simulator and the job submitters run.",
        platform_file, masterHostName);

    // Let's remove the master host from the hosts used to run jobs
    msg_host_t host;
    unsigned int i;
    xbt_dynar_foreach(all_hosts, i, host)
    {
        if (strcmp(MSG_host_get_name(host), masterHostName) == 0)
        {
            masterIndex = i;
            break;
        }
    }

    xbt_assert(masterIndex >= 0);
    xbt_dynar_remove_at(all_hosts, masterIndex, NULL);

    xbt_dynar_sort(all_hosts, machine_comparator);

    // Let's create a MSG process for each node
    xbt_dynar_foreach(all_hosts, i, host)
    {
        XBT_INFO("Create node process %d !", i);
        char * pname = NULL;
        int ret = asprintf(&pname, "node %d", i);
        xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
        MSG_process_create(pname, node, NULL, host);
        free(pname);
    }

    nb_nodes = xbt_dynar_length(all_hosts);
    createMachines(all_hosts);
    nodes = xbt_dynar_to_array(all_hosts);

    XBT_INFO("Nb nodes: %d", nb_nodes);

    // Let's create processes on the master host
    MSG_process_create("jobs_submitter", jobs_submitter, NULL, master_host);
    MSG_process_create("server", server, NULL, master_host);

    // We can now initialize the tracing and run the processes
    tracer = pajeTracer_create(pajeTraceFilename, 0, 32);
    pajeTracer_initialize(tracer, MSG_get_clock(), nb_nodes, nodes);

    msg_error_t res = MSG_main();

    freeMachines();
    pajeTracer_finalize(tracer, MSG_get_clock(), nb_nodes, nodes);
    pajeTracer_destroy(&tracer);

    exportJobsToCSV(csvJobsFilename);
    exportScheduleToCSV(csvScheduleFilename, microseconds_used_by_scheduler / (double)(1e6));

    XBT_INFO("Simulation time %g", MSG_get_clock());
    return res;
}

/**
 * @brief The main function arguments (a.k.a. program arguments)
 */
typedef struct s_main_args
{
    char * platformFilename;//! The SimGrid platform filename
    char * workloadFilename;//! The JSON workload filename

    char * socketFilename;  //! The Unix Domain Socket filename
    int nbConnectTries;     //! The number of connection tries to do on socketFilename
    double connectDelay;    //! The delay (in ms) between each connection try

    char * masterHostName;  //! The name of the SimGrid host which runs scheduler processes and not user tasks
    char * exportPrefix;    //! The filename prefix used to export simulation information

    int abort;              //! A boolean value. If set to yet, the launching should be aborted for reason abortReason
    char * abortReason;     //! Human readable reasons which explains why the launch should be aborted
} s_main_args_t;

/**
 * @brief Used to parse the main parameters
 * @param[in] key The current key
 * @param[in] arg The current argument
 * @param[in, out] state The current argp_state
 * @return 0
 */
static int parse_opt (int key, char *arg, struct argp_state *state)
{
    s_main_args_t * mainArgs = state->input;
    char * tmp = NULL;

    switch (key)
    {
    case 'd':
        mainArgs->connectDelay = atof(arg);
        if (mainArgs->connectDelay < 100 || mainArgs->connectDelay > 10000)
        {
            mainArgs->abort = 1;
            int ret = asprintf(&tmp, "\n  invalid connection delay (%lf): it must be in [100,10000] ms", mainArgs->connectDelay);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
            strcat(mainArgs->abortReason, tmp);
            free(tmp);
        }
        break;
    case 'e':
        mainArgs->exportPrefix = arg;
        break;
    case 'm':
        mainArgs->masterHostName = arg;
        break;
    case 's':
        mainArgs->socketFilename = arg;
        break;
    case 't':
        mainArgs->nbConnectTries = atoi(arg);
        if (mainArgs->nbConnectTries < 1)
        {
            mainArgs->abort = 1;
            int ret = asprintf(&tmp, "\n  invalid number of connection tries (%d): it must be greater than or equal to 1", mainArgs->nbConnectTries);
            xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
            strcat(mainArgs->abortReason, tmp);
            free(tmp);
        }
        break;
    case ARGP_KEY_ARG:
        switch(state->arg_num)
        {
        case 0:
            mainArgs->platformFilename = arg;
            if (access(mainArgs->platformFilename, R_OK) == -1)
            {
                mainArgs->abort = 1;
                int ret = asprintf(&tmp, "\n  invalid PLATFORM_FILE argument: file '%s' cannot be read", mainArgs->platformFilename);
                xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                strcat(mainArgs->abortReason, tmp);
                free(tmp);
            }
            break;
        case 1:
            mainArgs->workloadFilename = arg;
            if (access(mainArgs->workloadFilename, R_OK) == -1)
            {
                mainArgs->abort = 1;
                int ret = asprintf(&tmp, "\n  invalid WORKLOAD_FILE argument: file '%s' cannot be read", mainArgs->workloadFilename);
                xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
                strcat(mainArgs->abortReason, tmp);
                free(tmp);
            }
            break;
        }
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 2)
        {
            mainArgs->abort = 1;
            strcat(mainArgs->abortReason, "\n\tToo few arguments. Try the --help option to display usage information.");
        }
        /*else if (state->arg_num > 2)
        {
            mainArgs->abort = 1;
            strcat(mainArgs->abortReason, "\n\tToo many arguments.");
        }*/
        break;
    }

    return 0;
}

/**
 * @brief The main function (entry point of the program)
 * @param[in] argc The number of input arguments
 * @param[in] argv The input arguments
 * @return 0 if the simulation run successfully, something else otherwise
 */
int main(int argc, char *argv[])
{
    s_main_args_t mainArgs;
    mainArgs.socketFilename = "/tmp/bat_socket";
    mainArgs.nbConnectTries = 10;
    mainArgs.connectDelay = 1000;
    mainArgs.platformFilename = NULL;
    mainArgs.workloadFilename = NULL;
    mainArgs.masterHostName = "master_host";
    mainArgs.exportPrefix = "out";
    mainArgs.abort = 0;
    mainArgs.abortReason = xbt_new(char, 4096);
    mainArgs.abortReason[0] = '\0'; // Empties the string

    struct argp_option options[] =
    {
        {"socket", 's', "FILENAME", 0, "Unix Domain Socket filename", 0},
        {"connection-tries", 't', "NUM", 0, "The maximum number of connection tries", 0},
        {"connection-delay", 'd', "MS", 0, "The number of milliseconds between each connection try", 0},
        {"master-host", 'm', "NAME", 0, "The name of the host in PLATFORM_FILE which will run SimGrid scheduling processes and won't be used to compute tasks", 0},
        {"export", 'e', "FILENAME_PREFIX", 0, "The export filename prefix used to generate simulation output", 0},
        {0, '\0', 0, 0, 0, 0} // The options array must be NULL-terminated
    };
    struct argp argp = {options, parse_opt, "PLATFORM_FILE WORKLOAD_FILE", "A tool to simulate (via SimGrid) the behaviour of scheduling algorithms.", 0, 0, 0};
    argp_parse(&argp, argc, argv, 0, 0, &mainArgs);

    if (mainArgs.abort)
    {
        fprintf(stderr, "Impossible to run batsim:%s\n", mainArgs.abortReason);
        free(mainArgs.abortReason);
        return 1;
    }

    free (mainArgs.abortReason);

    json_t *json_workload_profile;

    //Comment to remove debug message
    xbt_log_control_set("batsim.threshold:debug");
    xbt_log_control_set("network.threshold:info");
    xbt_log_control_set("task.threshold:critical");

    json_workload_profile = load_json_workload_profile(mainArgs.workloadFilename);
    retrieve_jobs(json_workload_profile);
    retrieve_profiles(json_workload_profile);
    checkJobsAndProfilesValidity();

    MSG_init(&argc, argv);

    // Register all smpi jobs app and init SMPI
    bool smpi_used = register_smpi_app_instances();

    open_uds(mainArgs.socketFilename, mainArgs.nbConnectTries, mainArgs.connectDelay);

    char * pajeFilename;
    char * csvJobsFilename;
    char * csvScheduleFilename;

    int ret = asprintf(&pajeFilename, "%s_schedule.trace", mainArgs.exportPrefix);
    xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
    ret = asprintf(&csvJobsFilename, "%s_jobs.csv", mainArgs.exportPrefix);
    xbt_assert(ret != -1, "asprintf failed (not enough memory?)");
    ret = asprintf(&csvScheduleFilename, "%s_schedule.csv", mainArgs.exportPrefix);
    xbt_assert(ret != -1, "asprintf failed (not enough memory?)");

    msg_error_t res = deploy_all(mainArgs.platformFilename, mainArgs.masterHostName, pajeFilename, csvJobsFilename, csvScheduleFilename, smpi_used);

    free(pajeFilename);
    free(csvJobsFilename);
    free(csvScheduleFilename);

    json_decref(json_workload_profile);
    // Let's clear global allocated data
    freeJobStructures();
    free(nodes);

    //free(jobs);

    if (res == MSG_OK)
        return 0;
    else
        return 1;
}

/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or motask_data_tdify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "batsim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

#include <msg/msg.h>

#include <xbt/sysdep.h>         /* calloc, printf */
/* Create a log channel to have nice outputs. */
#include <xbt/log.h>
#include <xbt/asserts.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(batsim, "Batsim");

#include "job.h"
#include "utils.h"
#include "export.h"

#define BAT_SOCK_NAME "/tmp/bat_socket"

#define COMM_SIZE 0.000001 //#define COMM_SIZE 0.0 // => raise core dump 
#define COMP_SIZE 0.0

#define true 1
#define false 0

const int schedMessageMaxLength = 1024*1024 - 1; // 1 Mio should be enough...

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

int nb_nodes = 0;
msg_host_t *nodes;

int uds_fd = -1;

PajeTracer * tracer;

// process functions
static int node(int argc, char *argv[]);
static int server(int argc, char *argv[]);
static int launch_job(int argc, char *argv[]);
static int kill_job(int argc, char *argv[]);
static void send_message(const char *dst, e_task_type_t type, int job_id, void *data);

/**
 * \brief Send message through Unix Domain Socket
 */
static int send_uds(char *msg)
{
    int32_t lg = strlen(msg);
    write(uds_fd, &lg, 4);
    write(uds_fd, msg, lg);
    XBT_INFO("send_uds lg=%d, msg='%s'", lg, msg);

    return 0;
}

/**
 * \brief Receive message through Unix Domain Socket
 */

static char *recv_uds()
{
    int32_t lg;
    char *msg;
    read(uds_fd, &lg, 4);
    XBT_INFO("Received message length: %d bytes", lg);
    xbt_assert(lg > 0, "Invalid message received (size=%d)", lg);
    msg = (char *) malloc(sizeof(char)*(lg+1)); /* +1 for null terminator */
    read(uds_fd, msg, lg);
    msg[lg] = 0;
    XBT_INFO("Received message: '%s'", msg);

    return msg;
}

/**
 * \brief Open Unix Domain Socket for communication with scheduler
 */
static void open_uds()
{
    struct sockaddr_un address;

    uds_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(uds_fd < 0)
    {
        XBT_ERROR("socket() failed\n");
        exit(1);
    }
    /* start with a clean address structure */
    memset(&address, 0, sizeof(struct sockaddr_un));

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, 255, BAT_SOCK_NAME);

    if(connect(uds_fd,
               (struct sockaddr *) &address,
               sizeof(struct sockaddr_un)) != 0)
    {
        XBT_ERROR("connect() failed\n");
        exit(1);
    }
}

/**
 * @brief Sends a request to the scheduler (via UDS) and handles its reply.
 * @details This function sends a SCHED_EVENT by event received then a SCHED_READY when all events have been sent
 * @return 1
 */
static int requestReplyScheduler(int argc, char *argv[])
{
    char *message_send = MSG_process_get_data(MSG_process_self());
    XBT_INFO("Buffer received in REQ-REP: '%s'", message_send);

    char sendDateAsString[16];
    sprintf(sendDateAsString, "%f", MSG_get_clock());

    char * sendBuf;
    asprintf(&sendBuf, "0:%s%s", sendDateAsString, message_send);
    send_uds(sendBuf);

    free(sendBuf);
    sprintf(message_send, "");

    char * message_recv = recv_uds();
    xbt_dynar_t answer_dynar = xbt_str_split(message_recv, "|");

    int answerParts = xbt_dynar_length(answer_dynar);
    xbt_assert(answerParts >= 2, "Invalid message received ('%s'): it should be composed of at least 2 parts separated by a '|'", message_recv);

    double previousDate = atof(sendDateAsString);

    for (int i = 1; i < answerParts; ++i)
    {
        char * eventString = *(char **) xbt_dynar_get_ptr(answer_dynar, i);
        xbt_dynar_t * eventDynar = malloc(sizeof(xbt_dynar_t));
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
 * \brief
 */
void send_message(const char *dst, e_task_type_t type, int job_id, void *data)
{
    msg_task_t task_sent;
    s_task_data_t * req_data = malloc(sizeof(s_task_data_t));
    req_data->type = type;
    req_data->job_id = job_id;
    req_data->data = data;
    task_sent = MSG_task_create(NULL, COMP_SIZE, COMM_SIZE, req_data);

    XBT_INFO("message from '%s' to '%s' of type '%s' with data %p",
             MSG_process_get_name(MSG_process_self()), dst, task_type2str[type], data);

    MSG_task_send(task_sent, dst);
}

/**
 * \brief
 */
static void task_free(msg_task_t * task)
{
    if(*task != NULL)
    {
        //xbt_free(MSG_task_get_data(*task));
        MSG_task_destroy(*task);
        *task = NULL;
    }
}

/**
 * @brief The function in charge of job launching
 * @return 0
 */
static int launch_job(int argc, char *argv[])
{
    // Let's wait our data
    msg_task_t task_received = NULL;
    MSG_task_receive(&(task_received), MSG_process_get_name(MSG_process_self()));

    // Let's get our data
    s_task_data_t * task_data = (s_task_data_t *) MSG_task_get_data(task_received);
    xbt_assert(task_data->type == LAUNCHER_INFORMATION);

    s_launch_data_t * data = task_data->data;
    s_kill_data_t * kdata = data->killerData;
    int jobID = data->jobID;
    s_job_t * job = jobFromJobID(jobID);

    task_free(&task_received);
    free(task_data);

    XBT_INFO("Launching job %d", jobID);

    // Let's run the job
    pajeTracer_addJobLaunching(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);
    pajeTracer_addJobRunning(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);

    job_exec(jobID, data->reservedNodeCount, data->reservedNodesIDs, nodes, &(data->dataToRelease));
    
    //       .       |
    //      / \      |
    //     / | \     |
    //    /  |  \    | The remaining code of this function is executed ONLY IF
    //   /   |   \   | the job finished in time (its execution time was smaller than its walltime)
    //  /         \  |
    // /     *     \ |
    // ‾‾‾‾‾‾‾‾‾‾‾‾‾

    XBT_INFO("Job %d finished in time.", jobID);
    job->state = JOB_STATE_COMPLETED_SUCCESSFULLY;
    int dbg = 0;

    pajeTracer_addJobEnding(tracer, MSG_get_clock(), jobID, data->reservedNodeCount, data->reservedNodesIDs);

    // Let's kill the killer, destroy its associated data then our data
    XBT_INFO("Killing killer %d...", jobID);
    MSG_process_kill(data->killerProcess);
    free(kdata);
    xbt_dict_free(&(data->dataToRelease));
    free(data);

    XBT_INFO("Killing and freeing done");

    send_message("server", JOB_COMPLETED, jobID, NULL);

    return 0;
}

/**
 * @brief The function in charge of job killing
 * @return 0
 */
static int kill_job(int argc, char *argv[])
{
    // Let's wait our data
    msg_task_t task_received = NULL;
    MSG_task_receive(&(task_received), MSG_process_get_name(MSG_process_self()));

    // Let's get our data
    s_task_data_t * task_data = (s_task_data_t *) MSG_task_get_data(task_received);
    xbt_assert(task_data->type == KILLER_INFORMATION);

    s_kill_data_t * data = task_data->data;
    s_launch_data_t * ldata = data->launcherData;
    int jobID = ldata->jobID;
    s_job_t * job = jobFromJobID(jobID);
    double walltime = job->walltime;

    task_free(&task_received);
    free(task_data);

    XBT_INFO("Sleeping for %lf seconds to possibly kill job %d", walltime, jobID);

    // Let's sleep until the walltime is reached
    MSG_process_sleep(walltime);

    //       .
    //      / \      |
    //     / | \     |
    //    /  |  \    | The remaining code of this function is executed ONLY IF
    //   /   |   \   | the killer finished its wait before the launcher completion
    //  /         \  |
    // /     *     \ |
    // ‾‾‾‾‾‾‾‾‾‾‾‾‾

    XBT_INFO("Sleeping done. Job %d did NOT finish in time and must be killed", jobID);
    job->state = JOB_STATE_COMPLETED_KILLED;

    pajeTracer_addJobEnding(tracer, MSG_get_clock(), jobID, ldata->reservedNodeCount, ldata->reservedNodesIDs);
    pajeTracer_addJobKill(tracer, MSG_get_clock(), jobID, ldata->reservedNodeCount, ldata->reservedNodesIDs);

    // Let's kill the launcher, destroy its associated data then our data
    msg_task_t task = xbt_dict_get_or_null(ldata->dataToRelease, "task");
    if (task != NULL)
    {
        XBT_INFO("Cancelling the associated task");
        MSG_task_cancel(task);
        XBT_INFO("Task cancelled");
    }

    xbt_dict_free(&(ldata->dataToRelease));
    XBT_INFO("Killing launcher %d", jobID);
    MSG_process_kill(data->launcherProcess);
    free(ldata);
    free(data);

    XBT_INFO("Killing and freeing of job %d done", jobID);

    // Let's say to the server that the job execution finished
    send_message("server", JOB_COMPLETED, jobID, NULL);

    return 0;
}

/**
 * \brief
 */
static int node(int argc, char *argv[])
{
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
                s_kill_data_t * killData = (s_kill_data_t *) malloc(sizeof(s_kill_data_t));

                char * plname = NULL;
                char * pkname = NULL;
                asprintf(&plname, "launcher %d", launchData->jobID);
                asprintf(&pkname, "killer %d", launchData->jobID);

                // MSG process launching. These processes wait because their data is incomplete
                msg_process_t launcher = MSG_process_create(plname, launch_job, launchData, MSG_host_self());
                msg_process_t killer = MSG_process_create(pkname, kill_job, killData, MSG_host_self());

                // Now that both processes exist, they know each other
                launchData->killerProcess = killer;
                launchData->killerData = killData;
                killData->launcherProcess = launcher;
                killData->launcherData = launchData;

                // Let's send their data to the processes
                send_message(plname, LAUNCHER_INFORMATION, launchData->jobID, launchData);
                send_message(pkname, KILLER_INFORMATION, launchData->jobID, killData);

                free(plname);
                free(pkname);

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
    s_job_t * job;
    unsigned int job_index;

    send_message("server", SUBMITTER_HELLO, 0, NULL);

    // todo: read jobs here and sort them by ascending submission date
    double previousSubmissionDate = MSG_get_clock();

    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {
        if (job->submission_time < previousSubmissionDate)
            XBT_ERROR("The input workload JSON file is not sorted by ascending date, which is not handled yet");

        double timeToSleep = max(0, job->submission_time - previousSubmissionDate);
        MSG_process_sleep(timeToSleep);

        previousSubmissionDate = MSG_get_clock();
        send_message("server", JOB_SUBMITTED, job->id, NULL);
    }

    send_message("server", SUBMITTER_BYE, 0, NULL);

    return 0;
}


/**
 * \brief The central process which manage the global orchestration
 */
int server(int argc, char *argv[])
{
    msg_host_t node;

    msg_task_t task_received = NULL;
    s_task_data_t * task_data;

    int nb_completed_jobs = 0;
    int nb_submitted_jobs = 0;
    int nb_submitters = 0;
    int nb_submitters_finished = 0;
    int sched_ready = true;
    int size_m;
    char *tmp;
    char *job_ready_str;
    char *jobid_ready;
    char *res_str;
    char *job_id_str;
    double t = 0;
    unsigned int i, j;

    xbt_assert(schedMessageMaxLength > 0);
    char *sched_message = malloc(sizeof(char) * (schedMessageMaxLength+1)); // + 1 for NULL-terminated
    xbt_assert(sched_message != NULL, "Cannot allocate the send message buffer (requested bytes: %d)", schedMessageMaxLength+1);
    snprintf(sched_message, schedMessageMaxLength, "");

    // todo: add a better finition, for example the submitters could say "hello" and "goodbye" to the scheduler
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
            nb_completed_jobs++;
            s_job_t * job = jobFromJobID(task_data->job_id);

            XBT_INFO("Job %d COMPLETED. %d jobs completed so far", job->id, nb_completed_jobs);
            size_m = snprintf(sched_message, schedMessageMaxLength,
                              "%s|%f:C:%s", sched_message, MSG_get_clock(), job->id_str);
            xbt_assert(size_m <= schedMessageMaxLength, "Buffer for sending messages to the scheduler is not big enough...");
            XBT_INFO("Message to send to scheduler: %s", sched_message);

            break;
        } // end of case JOB_COMPLETED
        case JOB_SUBMITTED:
        {
            nb_submitted_jobs++;
            s_job_t * job = jobFromJobID(task_data->job_id);
            job->state = JOB_STATE_SUBMITTED;

            XBT_INFO("Job %d SUBMITTED. %d jobs submitted so far", job->id, nb_submitted_jobs);
            size_m = snprintf(sched_message, schedMessageMaxLength,
                              "%s|%f:S:%s", sched_message, MSG_get_clock(), job->id_str);
            xbt_assert(size_m <= schedMessageMaxLength, "Buffer for sending messages to the scheduler is not big enough...");
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
                                   "or already scheduled (state=%d)", jobID, job->state);
                        job->state = JOB_STATE_RUNNING;

                        xbt_dynar_t res_dynar = xbt_str_split(job_reservs_str, ",");

                        int head_node = atoi(*(char **)xbt_dynar_get_ptr(res_dynar, 0));
                        XBT_INFO("head node: %s, id: %d", MSG_host_get_name(nodes[head_node]), head_node);

                        // Let's create a launch data structure, which will be given to the head_node then used to launch the job
                        s_launch_data_t * launchData = (s_launch_data_t*) malloc(sizeof(s_launch_data_t));
                        launchData->jobID = jobID;
                        launchData->reservedNodeCount = xbt_dynar_length(res_dynar);

                        xbt_assert(launchData->reservedNodeCount == jobFromJobID(launchData->jobID)->nb_res,
                                   "Invalid scheduling algorithm decision: allocation of job %d is done on %d nodes (instead of %d)",
                                   launchData->jobID, launchData->reservedNodeCount, jobFromJobID(launchData->jobID)->nb_res);

                        launchData->reservedNodesIDs = (int*) malloc(launchData->reservedNodeCount * sizeof(int));
                        launchData->dataToRelease = xbt_dict_new();
                        xbt_dict_set(launchData->dataToRelease, "reservedNodesIDs", launchData->reservedNodesIDs, free);

                        // Let's fill the reserved node IDs from the XBT dynar
                        xbt_dynar_foreach(res_dynar, j, res_str)
                        {
                            int machineID = atoi(res_str);
                            launchData->reservedNodesIDs[j] = machineID;
                            xbt_assert(machineID >= 0 && machineID < nb_nodes,
                                       "Invalid machineID (%d) received from the scheduler: not in range [0;%d]", machineID, nb_nodes);
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
                XBT_DEBUG("Nothing to do");
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
            sched_ready = true;
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
            MSG_process_create("Request and reply scheduler", requestReplyScheduler, sched_message, MSG_host_self());
            sched_ready = false;
        }

    } // end of while

    // send finalize to node
    XBT_INFO("All jobs completed, time to finalize");

    for(i = 0; i < nb_nodes; i++)
        send_message(MSG_host_get_name(nodes[i]), FINALIZE, 0, NULL);

    free(sched_message);

    return 0;
}

/**
 * \brief
 */
msg_error_t deploy_all(const char *platform_file)
{
    MSG_config("workstation/model", "ptask_L07");
    MSG_create_environment(platform_file);

    xbt_dynar_t all_hosts = MSG_hosts_as_dynar();

    // Let's find the master host (the one on which the simulator and the job submitters run)
    const char * masterHostName = "master_host";
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

    // Let's create a MSG process for each node
    xbt_dynar_foreach(all_hosts, i, host)
    {
        XBT_INFO("Create node process %d !", i);
        char * pname = NULL;
        asprintf(&pname, "node %d", i);
        MSG_process_create(pname, node, NULL, host);
        free(pname);
    }

    nb_nodes = xbt_dynar_length(all_hosts);
    nodes = xbt_dynar_to_array(all_hosts);

    XBT_INFO("Nb nodes: %d", nb_nodes);

    // Let's create processes on the master host
    MSG_process_create("jobs_submitter", jobs_submitter, NULL, master_host);
    MSG_process_create("server", server, NULL, master_host);

    // We can now initialize the tracing and run the processes
    tracer = pajeTracer_create("schedule.trace", 0, 32);
    pajeTracer_initialize(tracer, MSG_get_clock(), nb_nodes, nodes);

    msg_error_t res = MSG_main();

    pajeTracer_finalize(tracer, MSG_get_clock(), nb_nodes, nodes);
    pajeTracer_destroy(&tracer);

    XBT_INFO("Simulation time %g", MSG_get_clock());
    return res;
}

int main(int argc, char *argv[])
{
    msg_error_t res = MSG_OK;
    int i;

    json_t *json_workload_profile;

    //Comment to remove debug message
    xbt_log_control_set("batsim.threshold:debug");

    if (argc < 2)
    {
        printf("Batsim: Batch System Simulator.\n");
        printf("\n");
        printf("Usage: %s platform_file workload_file\n", argv[0]);
        printf("example: %s platforms/small_platform.xml workload_profiles/test_workload_profile.json\n", argv[0]);
        exit(1);
    }

    json_workload_profile = load_json_workload_profile(argv[2]);
    retrieve_jobs(json_workload_profile);
    retrieve_profiles(json_workload_profile);

    open_uds();

    MSG_init(&argc, argv);

    res = deploy_all(argv[1]);

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

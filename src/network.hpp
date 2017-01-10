/**
 * @file network.hpp
 * @brief Contains network-related classes and functions
 */

#pragma once

#include <string>

#include "ipp.hpp"
struct BatsimContext;

/**
 * @brief Contains the different network stamps used in the network protocol
 */
enum NetworkStamp : char
{
    STATIC_JOB_ALLOCATION = 'J' //!< Decision -> Batsim. Allows the Decision real process to take a scheduling decision: To decide where a job should be executed now
    ,JOB_REJECTION = 'R' //!< Decision -> Batsim. Allows the Decision real process to reject a job (the job will not be computed)
    ,NOP = 'N' //!< Decision <-> Batsim. Does nothing. Since the network protocol is synchronous, the simulation would be stopped if a network component (Batsim or the Decision real process) stopped sending messages. That's what this stamp is used for.
    ,STATIC_JOB_SUBMISSION = 'S' //!< Batsim -> Decision. Batsim tells the Decision real process that a static job (one in the initial workload) has been submitted
    ,STATIC_JOB_COMPLETION = 'C' //!< Batsim -> Decision. Batsim tells the Decision real process that a static job (one in the initial workload) has been completed (finished its execution)
    ,PSTATE_SET = 'P' //!< Decision -> Batsim. The Decision real process wants to change the power state of one or several machines
    ,NOP_ME_LATER = 'n' //!< Decision -> Batsim. The Decision real process wants to be awaken at a future simulation time
    ,TELL_ME_CONSUMED_ENERGY = 'E' //!< Decision -> Batsim. The Decision real process wants to know how much energy has been consumed on computing machines since the beginning of the simulation
    ,PSTATE_HAS_BEEN_SET = 'p' //!< Batsim -> Decision. Batsim acknowledges that the power state of one of several machines has been changed
    ,QUERY_WAIT = 'Q' //!< Batsim -> Decision. Batsim queries the Decision process on the waiting time to get a given amount of resources
    ,CONSUMED_ENERGY = 'e' //!< Batsim -> Decision. Batsim tells the Decision process how much energy has been used since the beginning of the simulation
    ,ANSWER_WAIT = 'W' //!< Decision -> Batsim. The Decision process answers to Batsim on the waiting time to get a given amount of resources
};

/**
 * @brief Handles the socket used in the network protocol
 */
class UnixDomainSocket
{
public:
    /**
     * @brief Creates a UnixDomainSocket, whose internal sockets do not exist
     */
    UnixDomainSocket();

    /**
     * @brief Creates a UnixDomainSocket, whose internal server socket is created via create_socket
     * @param[in] filename The name of the file that is used as the Unix Domain Socket
     */
    UnixDomainSocket(const std::string & filename);

    /**
     * @brief Destroys a UnixDomainSocket, closing open sockets
     */
    ~UnixDomainSocket();

    /**
     * @brief Creates the server socket
     * @brief This method unlinks the file, creates a socket, binds it and starts listening on it
     * @param[in] filename The name of the file that is used as the Unix Domain Socket
     */
    void create_socket(const std::string & filename);

    /**
     * @brief Accept one pending connection (wait for it if none has been done yet)
     */
    void accept_pending_connection();

    /**
     * @brief Closes the internal sockets
     */
    void close();

    /**
     * @brief Waits for a message on the client socket (from the Decision real process) then returns it
     * @return The message received on the client socket
     */
    std::string receive();

    /**
     * @brief Sends a message on the client socket (to the Decision real process)
     * @param[in] message The message to send
     */
    void send(const std::string & message);

private:
    int _server_socket = -1; //!< The server-side socket
    int _client_socket = -1; //!< The client-side socket
};

/**
 * @brief The process in charge of doing a Request-Reply iteration with the Decision real process
 * @details This process sends a message to the Decision real process (Request) then waits for the answered message (Reply)
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int request_reply_scheduler_process(int argc, char *argv[]);

/**
 * @brief Computes the absolute filename of a given file
 * @param[in] filename The name of the file (not necessarily existing).
 * @return The absolute filename corresponding to the given filename
 */
std::string absolute_filename(const std::string & filename);

/**
 * @brief Retrieves the workload_name and the job_id from a job_identifier
 * @details Job identifiers are in the form [WORKLOAD_NAME!]JOB_ID
 * @param[in] context The BatsimContext
 * @param[in] job_identifier_string The input job identifier string
 * @param[out] job_id The output JobIdentifier
 * @return true if the info has been retrieved successfully and that the job exists, false otherwise
 */
bool identify_job_from_string(BatsimContext * context,
                              const std::string & job_identifier_string,
                              JobIdentifier & job_id);

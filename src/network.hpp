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
 * @brief The process in charge of doing a Request-Reply iteration with the Decision real process
 * @details This process sends a message to the Decision real process (Request) then waits for the answered message (Reply)
 * @param[in] context The BatsimContext
 * @param[in] send_buffer The message to send to the Decision real process
 */
void request_reply_scheduler_process(BatsimContext *context, std::string send_buffer);

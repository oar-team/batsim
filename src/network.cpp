/**
 * @file network.cpp
 * @brief Contains network-related classes and functions
 */

#include "network.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include <zmq.h>

#include "context.hpp"
#include "ipp.hpp"
#include "protocol.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(network, "network"); //!< Logging

using namespace std;

/**
 * @brief The process in charge of doing a Request-Reply iteration with the Decision real process
 * @details This process sends a message to the Decision real process (Request) then waits for the answered message (Reply)
 * @param[in] context The BatsimContext
 * @param[in] send_buffer The message to send to the Decision real process
 */
void request_reply_scheduler_process(BatsimContext * context, std::string send_buffer) // TODO: no copy!
{
    XBT_DEBUG("Buffer received in REQ-REP: '%s'", send_buffer.c_str());

    try
    {
        string message_received;
        uint8_t * msg_buffer = nullptr;
        auto start = chrono::steady_clock::now();

        if (context->zmq_socket != nullptr)
        {
            // Send the message on the socket
            XBT_INFO("Sending '%s'", send_buffer.c_str());
            if (zmq_send(context->zmq_socket, send_buffer.data(), send_buffer.size(), 0) == -1)
                throw std::runtime_error(std::string("Cannot send message on socket (errno=") + strerror(errno) + ")");

            // Get the reply
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            if (zmq_msg_recv(&msg, context->zmq_socket, 0) == -1)
                throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

            message_received = std::string(static_cast<char*>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
            msg_buffer = (uint8_t *)message_received.c_str();
            XBT_INFO("Received '%s'", message_received.c_str());
        }
        else
        {
            // Call the external library
            if (context->edc_library->take_decisions((uint8_t *)send_buffer.c_str(), &msg_buffer) != 0)
                throw std::runtime_error("Error while calling take_decisions on the external library");
        }

        auto end = chrono::steady_clock::now();
        long double elapsed_microseconds = static_cast<long double>(chrono::duration <long double, micro> (end - start).count());
        context->microseconds_used_by_scheduler += elapsed_microseconds;

        double now = -1;
        std::vector<IPMessageWithTimestamp> messages;
        protocol::parse_batprotocol_message(msg_buffer, now, messages, context);

        for (unsigned int i = 0; i < messages.size(); ++i)
        {
            send_message_at_time("server", messages[i].message, messages[i].timestamp, false);
        }

        send_message_at_time("server", IPMessageType::SCHED_READY, nullptr, now, false);
    }
    catch(const std::runtime_error & error)
    {
        XBT_INFO("Runtime error received: %s", error.what());
        XBT_INFO("Flushing output files...");

        finalize_batsim_outputs(context);

        XBT_INFO("Output files flushed. Aborting execution now.");
        throw runtime_error("Execution aborted (connection broken)");
    }
}

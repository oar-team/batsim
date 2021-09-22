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
void request_reply_scheduler_process(BatsimContext * context, uint8_t * data, uint32_t data_size)
{
    try
    {
        uint8_t * msg_buffer = nullptr;
        uint32_t msg_buffer_size = 0u;
        auto start = chrono::steady_clock::now();

        if (context->edc_json_format)
        {
            XBT_INFO("Sending '%s'", (const char*) data);
        }

        if (context->zmq_socket != nullptr)
        {
            // Send the message on the socket
            if (zmq_send(context->zmq_socket, data, data_size, 0) == -1)
                throw std::runtime_error(std::string("Cannot send message on socket (errno=") + strerror(errno) + ")");

            // Get the reply
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            if (zmq_msg_recv(&msg, context->zmq_socket, 0) == -1)
                throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

            msg_buffer = (uint8_t *)zmq_msg_data(&msg);
            msg_buffer_size = zmq_msg_size(&msg);
        }
        else
        {
            // Call the external library
            uint8_t return_code = 0u;
            try
            {
                XBT_DEBUG("Calling the external library");
                return_code = context->edc_library->take_decisions(data, data_size, &msg_buffer, &msg_buffer_size);
                XBT_DEBUG("External library call finished");
            }
            catch (const std::exception & e)
            {
                throw std::runtime_error("Exception thrown by the external library take_decisions function: " + std::string(e.what()));
            }

            if (return_code != 0)
            {
                throw std::runtime_error("Error while calling take_decisions on the external library: returned " + std::to_string(return_code));
            }
        }

        auto end = chrono::steady_clock::now();
        long double elapsed_microseconds = static_cast<long double>(chrono::duration <long double, micro> (end - start).count());
        context->microseconds_used_by_scheduler += elapsed_microseconds;

        if (context->edc_json_format)
        {
            XBT_INFO("Received '%s'", (char *)msg_buffer);
        }

        free(data);
        data = nullptr;

        double now = -1;
        std::vector<IPMessageWithTimestamp> messages;
        protocol::parse_batprotocol_message(msg_buffer, msg_buffer_size, now, messages, context);

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

        free(data);
        data = nullptr;

        finalize_batsim_outputs(context);

        XBT_INFO("Output files flushed. Aborting execution now.");
        throw runtime_error("Execution aborted (connection broken)");
    }
}

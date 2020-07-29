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

XBT_LOG_NEW_DEFAULT_CATEGORY(network, "network"); //!< Logging

using namespace std;

void request_reply_scheduler_process(BatsimContext * context, std::string send_buffer)
{
    XBT_DEBUG("Buffer received in REQ-REP: '%s'", send_buffer.c_str());

    try
    {
        // TODO: Make sure the message is sent as UTF-8?
        string message_to_send = send_buffer;

        // Send the message
        XBT_INFO("Sending '%s'", message_to_send.c_str());
        if (zmq_send(context->zmq_socket, message_to_send.data(), message_to_send.size(), 0) == -1)
            throw std::runtime_error(std::string("Cannot send message on socket (errno=") + strerror(errno) + ")");

        auto start = chrono::steady_clock::now();
        string message_received;

        // Get the reply
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        if (zmq_msg_recv(&msg, context->zmq_socket, 0) == -1)
            throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

        string raw_message_received(static_cast<char*>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
        message_received = raw_message_received;
        XBT_INFO("Received '%s'", message_received.c_str());

        auto end = chrono::steady_clock::now();
        long double elapsed_microseconds = static_cast<long double>(chrono::duration <long double, micro> (end - start).count());
        context->microseconds_used_by_scheduler += elapsed_microseconds;

        context->proto_reader->parse_and_apply_message(message_received);
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

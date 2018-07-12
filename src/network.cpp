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

#include <boost/locale.hpp>

#include <simgrid/msg.h>

#include "context.hpp"
#include "ipp.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(network, "network"); //!< Logging

using namespace std;

int request_reply_scheduler_process(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    RequestReplyProcessArguments * args = (RequestReplyProcessArguments *) MSG_process_get_data(MSG_process_self());
    BatsimContext * context = args->context;

    XBT_DEBUG("Buffer received in REQ-REP: '%s'", args->send_buffer.c_str());

    // Let's make sure the message is sent as UTF-8
    string message_to_send = args->send_buffer;

    // Send the message
    XBT_INFO("Sending '%s'", message_to_send.c_str());
    context->zmq_socket->send(message_to_send.data(), message_to_send.size());

    auto start = chrono::steady_clock::now();
    string message_received;

    try
    {
        // Get the reply
        zmq::message_t reply;
        context->zmq_socket->recv(&reply);

        string raw_message_received((char *)reply.data(), reply.size());
        message_received = raw_message_received;
        XBT_INFO("Received '%s'", message_received.c_str());
    }
    catch(const std::runtime_error & error)
    {
        XBT_INFO("Runtime error received: %s", error.what());
        XBT_INFO("Flushing output files...");

        finalize_batsim_outputs(context);

        XBT_INFO("Output files flushed. Aborting execution now.");
        throw runtime_error("Execution aborted (connection broken)");
    }

    auto end = chrono::steady_clock::now();
    Rational elapsed_microseconds = (double) chrono::duration <long double, micro> (end - start).count();
    context->microseconds_used_by_scheduler += elapsed_microseconds;

    context->proto_reader->parse_and_apply_message(message_received);

    delete args;
    return 0;
}

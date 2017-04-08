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

    const int date_buf_size = 32;
    char sending_date_as_string[date_buf_size];
    int nb_printed_char = snprintf(sending_date_as_string, date_buf_size,
                                   "%f", MSG_get_clock());
    xbt_assert(nb_printed_char < date_buf_size - 1,
               "The date is now written as a %d-character long string, but the date "
               "buffer size is %d. Since information will be lost now or in a near "
               "future, the simulation is aborted.",
               nb_printed_char, date_buf_size);
    char *send_buf = (char*) args->send_buffer.c_str();
    XBT_DEBUG("Buffer received in REQ-REP: '%s'", send_buf);

    // Let's make sure the message is sent as UTF-8
    string utf8_message = boost::locale::conv::to_utf<char>(send_buf, "UTF-8");

    // Send the message
    XBT_INFO("Sending '%s'", utf8_message.c_str());
    context->zmq_socket->send(utf8_message.data(), utf8_message.size());

    auto start = chrono::steady_clock::now();
    string message_received;

    try
    {
        // Get the reply
        zmq::message_t reply;
        context->zmq_socket->recv(&reply);

        string raw_message_received((char *)reply.data(), reply.size());
        message_received = boost::locale::conv::from_utf(raw_message_received, "UTF-8");
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

std::string absolute_filename(const std::string & filename)
{
    xbt_assert(filename.length() > 0);

    // Let's assume filenames starting by "/" are absolute.
    if (filename[0] == '/')
        return filename;

    char cwd_buf[PATH_MAX];
    char * getcwd_ret = getcwd(cwd_buf, PATH_MAX);
    xbt_assert(getcwd_ret == cwd_buf, "getcwd failed");

    return string(getcwd_ret) + '/' + filename;
}

bool identify_job_from_string(BatsimContext * context,
                              const std::string & job_identifier_string,
                              JobIdentifier & job_id)
{
    // Let's split the job_identifier by '!'
    vector<string> job_identifier_parts;
    boost::split(job_identifier_parts, job_identifier_string, boost::is_any_of("!"), boost::token_compress_on);

    if (job_identifier_parts.size() == 1)
    {
        XBT_WARN("Job ID is not of format WORKLOAD!NUMBER... assuming static!");
        job_id.workload_name = "static";
        job_id.job_number = std::stoi(job_identifier_parts[0]);
    }
    else if (job_identifier_parts.size() == 2)
    {
        job_id.workload_name = job_identifier_parts[0];
        job_id.job_number = std::stoi(job_identifier_parts[1]);
    }
    else
        return false;

    return context->workloads.job_exists(job_id);
}

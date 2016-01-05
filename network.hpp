#pragma once

#include <string>
struct BatsimContext;

enum NetworkStamp : char
{
    STATIC_JOB_ALLOCATION = 'J',
    JOB_REJECTION = 'R',
    NOP = 'N',
    STATIC_JOB_SUBMISSION = 'S',
    STATIC_JOB_COMPLETION = 'C',
    PSTATE_SET = 'P',
    NOP_ME_LATER = 'n',

    PSTATE_HAS_BEEN_SET = 'p'
};

class UnixDomainSocket
{
public:
    UnixDomainSocket();
    UnixDomainSocket(const std::string & filename);
    ~UnixDomainSocket();

    void create_socket(const std::string & filename);
    void accept_pending_connection();
    void close();

    std::string receive();
    void send(const std::string & message);

private:
    int _server_socket;
    int _client_socket;
};

int request_reply_scheduler_process(int argc, char *argv[]);

#include <cassert>
#include <zmq.h>
#include <errno.h>
#include <string.h>

#include <CLI/CLI.hpp>

#include <stdio.h>

bool simulation_ends_received = false;

// copypaste from fcfs, but stores when batsim asks for simulation to finish
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>

#include <batprotocol.hpp>
#include <intervalset.hpp>
#include <nlohmann/json.hpp>

#include "batsim_edc.h"

using namespace batprotocol;
using json = nlohmann::json;

struct Job
{
    std::string id;
    uint32_t nb_hosts;
    IntervalSet alloc;
};

MessageBuilder * mb = nullptr;
bool format_binary = true; // whether flatbuffers binary or json format should be used

uint32_t platform_nb_hosts = 0;
std::list<::Job*> job_queue;
std::unordered_map<std::string, ::Job*> running_jobs;
uint32_t nb_available_hosts = 0;
IntervalSet available_hosts;

uint8_t batsim_edc_init(const uint8_t *init_data, uint32_t init_size, uint32_t *flags, uint8_t **reply_data, uint32_t *reply_size)
{
    std::string init_string((const char *)init_data, static_cast<size_t>(init_size));
    try {
        auto init_json = json::parse(init_string);

        if (init_json.contains("format_json"))
        {
            format_binary = !(init_json["format_json"]);
        }
    } catch (const json::exception & e) {
        throw std::runtime_error("scheduler called with bad init string: " + std::string(e.what()));
    }

    mb = new MessageBuilder(!format_binary);
    *flags = format_binary ? BATSIM_EDC_FORMAT_BINARY : BATSIM_EDC_FORMAT_JSON;

    mb->add_edc_hello("fcfs", "0.1.0");
    mb->finish_message(0.0);
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(reply_data), reply_size);

    return 0;
}

uint8_t batsim_edc_deinit()
{
    delete mb;
    mb = nullptr;

    return 0;
}

uint8_t batsim_edc_take_decisions(
    const uint8_t * what_happened,
    uint32_t what_happened_size,
    uint8_t ** decisions,
    uint32_t * decisions_size)
{
    (void) what_happened_size;
    auto * parsed = deserialize_message(*mb, !format_binary, what_happened);
    mb->clear(parsed->now());

    bool need_scheduling = false;
    auto nb_events = parsed->events()->size();
    for (unsigned int i = 0; i < nb_events; ++i)
    {
        auto event = (*parsed->events())[i];
        switch (event->event_type())
        {
        case fb::Event_SimulationEndsEvent: {
          simulation_ends_received = true;
        } break;
        case fb::Event_SimulationBeginsEvent: {
            auto simu_begins = event->event_as_SimulationBeginsEvent();
            platform_nb_hosts = simu_begins->computation_host_number();
            nb_available_hosts = platform_nb_hosts;
            available_hosts = IntervalSet::ClosedInterval(0, platform_nb_hosts - 1);
        } break;
        case fb::Event_JobSubmittedEvent: {
            ::Job job{
                event->event_as_JobSubmittedEvent()->job_id()->str(),
                event->event_as_JobSubmittedEvent()->job()->resource_request(),
                IntervalSet::empty_interval_set()
            };

            if (job.nb_hosts > platform_nb_hosts)
                mb->add_reject_job(job.id);
            else {
                need_scheduling = true;
                job_queue.emplace_back(new ::Job(job));
            }
        } break;
        case fb::Event_JobCompletedEvent: {
            need_scheduling = true;

            auto job_id = event->event_as_JobCompletedEvent()->job_id()->str();
            auto job_it = running_jobs.find(job_id);
            auto job = job_it->second;
            nb_available_hosts += job->nb_hosts;
            available_hosts += job->alloc;

            delete job;
            running_jobs.erase(job_it);
        } break;
        default: break;
        }
    }

    if (need_scheduling) {
        for (auto job_it = job_queue.begin(); job_it != job_queue.end(); ) {
            auto job = *job_it;
            if (job->nb_hosts <= nb_available_hosts) {
                running_jobs[job->id] = *job_it;
                job->alloc = available_hosts.left(job->nb_hosts);
                mb->add_execute_job(job->id, job->alloc.to_string_hyphen());
                available_hosts -= job->alloc;
                nb_available_hosts -= job->nb_hosts;

                job_it = job_queue.erase(job_it);
            }
            else
                break;
        }
    }

    mb->finish_message(parsed->now());
    serialize_message(*mb, !format_binary, const_cast<const uint8_t **>(decisions), decisions_size);
    return 0;
}


int main(int argc, char * argv[]) {
  CLI::App app{"Process EDC meant to test batsim's process interface"};

  std::string socket_endpoint;
  app.add_option("-s,--socket-endpoint", socket_endpoint, "")
    ->option_text("<endpoint>")
    ->required()
    ->description("Sets which EDC library should be loaded to take the decisions");

  try
  {
    app.parse(argc, argv);
  }
  catch(const CLI::ParseError & e)
  {
    return app.exit(e);
  }

  void * zmq_context = zmq_ctx_new();
  if (zmq_context == NULL)
    throw std::runtime_error("zmq_ctx_new failed, aborting");
  void * socket = zmq_socket(zmq_context, ZMQ_REP);
  if (socket == NULL)
    throw std::runtime_error("zmq_socket failed, aborting");
  printf("binding socket on %s\n", socket_endpoint.c_str());
  if (zmq_bind(socket, socket_endpoint.c_str()) != 0)
    throw std::runtime_error("zmq_bind failed, aborting");

  // initialization
  zmq_msg_t msg_init;
  zmq_msg_init(&msg_init);
  printf("waiting for init message... "); fflush(stdout);
  if (zmq_msg_recv(&msg_init, socket, 0) == -1)
      throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");
  printf("received. "); fflush(stdout);

  uint32_t buffer_size = zmq_msg_size(&msg_init);
  if (buffer_size < 4)
      throw std::runtime_error(std::string("Length of init message is too small"));

  uint8_t * buffer = (uint8_t *)zmq_msg_data(&msg_init);
  buffer_size = *(uint32_t*)buffer;
  buffer += 4;

  uint32_t flags;
  uint8_t * reply_data;
  uint32_t reply_size;
  uint32_t ret = batsim_edc_init(buffer, buffer_size, &flags, &reply_data, &reply_size);
  if (ret != 0)
    throw std::runtime_error("call to edc init returned non-zero, aborting");
  printf("reply generated. "); fflush(stdout);
  zmq_msg_close(&msg_init);

  zmq_msg_t msg_init_reply;
  int rc = zmq_msg_init_size(&msg_init_reply, reply_size + 4);
  if (rc != 0)
    throw std::runtime_error("could not allocate zmq reply message");
  buffer = static_cast<uint8_t *>(zmq_msg_data(&msg_init_reply));
  memcpy(buffer, &flags, 4);
  if (reply_size > 0)
    memcpy(buffer + 4, reply_data, reply_size);

  rc = zmq_msg_send(&msg_init_reply, socket, 0);
  if (rc != static_cast<int>(reply_size + 4))
    throw std::runtime_error(std::string("Cannot send init reply msg on socket (errno=") + strerror(errno) + ")");
  zmq_msg_close(&msg_init_reply);
  printf("and sent!\n"); fflush(stdout);

  // loop
  while (!simulation_ends_received) {
    printf("waiting for message... "); fflush(stdout);
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    if (zmq_msg_recv(&msg, socket, 0) == -1)
      throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");
    printf("received. "); fflush(stdout);
    buffer_size = zmq_msg_size(&msg);
    buffer = static_cast<uint8_t *>(zmq_msg_data(&msg));
    ret = batsim_edc_take_decisions(buffer, buffer_size, &reply_data, &reply_size);
    if (ret != 0)
      throw std::runtime_error("call to edc take_decisions returned non-zero, aborting");
    printf("decisions taken. "); fflush(stdout);
    zmq_msg_close(&msg);

    zmq_msg_t msg_reply;
    rc = zmq_msg_init_size(&msg_reply, reply_size);
    if (rc != 0)
      throw std::runtime_error("could not allocate zmq reply message");
    buffer = static_cast<uint8_t *>(zmq_msg_data(&msg_reply));
    if (reply_size > 0)
      memcpy(buffer, reply_data, reply_size);

    rc = zmq_msg_send(&msg_reply, socket, 0);
    if (rc != static_cast<int>(reply_size))
      throw std::runtime_error(std::string("Cannot send reply msg on socket (errno=") + strerror(errno) + ")");
    zmq_msg_close(&msg_reply);
    printf("and sent!\n"); fflush(stdout);
  }

  // deinit
  batsim_edc_deinit();
  if (zmq_unbind(socket, socket_endpoint.c_str()) != 0)
    perror("zmq_unbind");

  if (zmq_close(socket) != 0)
    perror("zmq_close");

  if (zmq_ctx_destroy(zmq_context) != 0)
    perror("zmq_ctx_destroy");

  return 0;
}

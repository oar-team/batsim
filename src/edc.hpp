#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "batsim.hpp"
#include "context.hpp"
#include "ipp.hpp"

namespace batprotocol
{
    class MessageBuilder;
}

/**
 * @brief Enumeration of possible external decision component types
 */
enum class EDCType
{
    LIBRARY //!< an ExternalLibrary
   ,PROCESS //!< an ExternalProcess
};

/**
 * @brief A variant type that wraps a way to call any kind of external decision component
 */
struct ExternalDecisionComponent
{
    static ExternalDecisionComponent * new_library(
        const std::string & lib_path,
        const EdcLibraryLoadMethod & load_method,
        const uint8_t * init_data,
        uint32_t init_data_size,
        uint32_t init_flags
    );
    static ExternalDecisionComponent * new_process(void * zmq_context, const std::string & connection_endpoint);
    ~ExternalDecisionComponent();

    void call_take_decisions(uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size);

private:
    ExternalDecisionComponent(EDCType type, void * component);

private:
    EDCType _type; //!< The type of external decision component
    void * _component; //!< The actual data behind this variant type
};

/**
 * @brief A structure to call an External Decision Component as a library from a C API.
 */
struct ExternalLibrary
{
    friend struct ExternalDecisionComponent;
    ExternalLibrary(const std::string & lib_path, const EdcLibraryLoadMethod & load_method);
    ~ExternalLibrary();

    void call_take_decisions(uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size);

private:
    void * _lib_handle = nullptr; //!< The library handle, as returned by dlmopen.
    uint8_t (*init)(const uint8_t *, uint32_t, uint32_t) = nullptr; //!< A function pointer to the batsim_edc_init symbol in the loaded library.
    uint8_t (*deinit)() = nullptr; //!< A function pointer to the batsim_edc_deinit symbol in the loaded library.
    uint8_t (*take_decisions)(const uint8_t*, uint32_t, uint8_t**, uint32_t*) = nullptr; //!< A function pointer to the batsim_edc_take_decisions symbol in the loaded library.
};

/**
 * @brief A structure to call an External Decision Component as a process via a ZMQ remote procedure call.
 */
struct ExternalProcess
{
    ExternalProcess(void * zmq_context, const std::string & connection_endpoint);
    ~ExternalProcess();

    void call_take_decisions(uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size);

private:
    void * _zmq_socket = nullptr; //!< The ZeroMQ socket associated with the ExternalProcess
};

void * load_lib_symbol(void * lib_handle, const char * symbol);

void zmq_call_take_decisions(void * zmq_socket, uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size);

void edc_decisions_injector(std::shared_ptr<std::vector<IPMessageWithTimestamp> > messages, double now);

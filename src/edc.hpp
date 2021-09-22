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
 * @brief A structure to call an External Decision Component as a library from a C API.
 */
struct ExternalLibrary
{
    void * lib_handle = nullptr; //!< The library handle, as returned by dlmopen.
    uint8_t (*init)(const uint8_t *, uint32_t, uint8_t) = nullptr; //!< A function pointer to the batsim_edc_init symbol in the loaded library.
    uint8_t (*deinit)() = nullptr; //!< A function pointer to the batsim_edc_deinit symbol in the loaded library.
    uint8_t (*take_decisions)(const uint8_t*, uint32_t, uint8_t**, uint32_t*) = nullptr; //!< A function pointer to the batsim_edc_take_decisions symbol in the loaded library.

    ExternalLibrary(const std::string & lib_path, const EdcLibraryLoadMethod & load_method);
    ~ExternalLibrary();

    void call_take_decisions(uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size);
};

void * load_lib_symbol(void * lib_handle, const char * symbol);

void zmq_call_take_decisions(void * zmq_socket, uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size);

void edc_decisions_injector(std::shared_ptr<std::vector<IPMessageWithTimestamp> > messages, double now);

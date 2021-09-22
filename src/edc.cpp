#include "edc.hpp"

#include <dlfcn.h>
#include <string.h>

#include <stdexcept>

#include <zmq.h>

#include <batprotocol.hpp>

#include "protocol.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(edc, "edc"); //!< Logging

/**
 * @brief Build an ExternalLibrary from a library path.
 * @param[in] lib_path The path of the library to load as an External Decision Component.
 */
ExternalLibrary::ExternalLibrary(const std::string & lib_path, const EdcLibraryLoadMethod & load_method)
{
    switch (load_method)
    {
    case EdcLibraryLoadMethod::DLMOPEN: {
        // dlmopen(LM_ID_NEWLM) places the library in a new memory namespace just for it.
        // this makes sure that the library and all their dependencies are:
        // - loaded into memory, which would not be done if similar libraries existed in the default (batsim's) namespace.
        // - loaded from the desired path/at the desired version if specified in the loaded ELF (e.g., via DT_RUNPATH).
        // - privatized, that is to say that their global variables are not shared between different components.
        lib_handle = dlmopen(LM_ID_NEWLM, lib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    } break;
    case EdcLibraryLoadMethod::DLOPEN: {
        // dlopen places the library in the default memory namespace.
        // - this may have collision with Batsim's memory (e.g., batprotocol-cpp)
        // - this is strongly discouraged if several EDCs should be loaded
        lib_handle = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    } break;
    }

    xbt_assert(lib_handle != NULL, "dlmopen failed while loading external decision component library: %s", dlerror());

    init = (uint8_t (*)(const uint8_t*, uint32_t, uint8_t)) load_lib_symbol(lib_handle, "batsim_edc_init");
    deinit = (uint8_t (*)()) load_lib_symbol(lib_handle, "batsim_edc_deinit");
    take_decisions = (uint8_t (*)(const uint8_t*, uint32_t, uint8_t**, uint32_t*)) load_lib_symbol(lib_handle, "batsim_edc_take_decisions");

    XBT_INFO("loaded external decision component library from '%s'", lib_path.c_str());
}

/**
 * @brief Deallocate an ExternalLibrary.
 */
ExternalLibrary::~ExternalLibrary()
{
    deinit();
    dlclose(lib_handle);
}

/**
 * @brief Load a symbol from a library handle.
 * @details Just a wrapper around dlsym.
 * @param[in] lib_handle The library handle where the symbol should be loaded
 * @param[in] symbol The symbol to load
 * @return The address of the symbol loaded
 */
void * load_lib_symbol(void * lib_handle, const char * symbol)
{
    void * address = dlsym(lib_handle, symbol);
    xbt_assert(address != NULL, "Could not load symbol '%s': %s", symbol, dlerror());
    return address;
}

void ExternalLibrary::call_take_decisions(uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t ** decisions_buffer, uint32_t * decisions_buffer_size)
{
    uint8_t return_code = 0u;
    try
    {
        XBT_DEBUG("Calling the external library");
        return_code = take_decisions(what_happened_buffer, what_happened_buffer_size, decisions_buffer, decisions_buffer_size);
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

void zmq_call_take_decisions(void *zmq_socket, uint8_t *what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t **decisions_buffer, uint32_t *decisions_buffer_size)
{
    // Send the message on the socket
    if (zmq_send(zmq_socket, what_happened_buffer, what_happened_buffer_size, 0) == -1)
        throw std::runtime_error(std::string("Cannot send message on socket (errno=") + strerror(errno) + ")");

    // Wait & read the reply on the socket
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    if (zmq_msg_recv(&msg, zmq_socket, 0) == -1)
        throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

    *decisions_buffer = (uint8_t *)zmq_msg_data(&msg);
    *decisions_buffer_size = zmq_msg_size(&msg);
}

void edc_decisions_injector(std::shared_ptr<std::vector<IPMessageWithTimestamp> > messages, double now)
{
    for (const auto & message : *messages.get())
    {
        send_message_at_time("server", message.message, message.timestamp, false);
    }

    send_message_at_time("server", IPMessageType::SCHED_READY, nullptr, now, false);
}

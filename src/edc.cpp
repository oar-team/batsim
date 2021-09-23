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
 * @param[in] load_method How the library should be loaded into memory
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
        _lib_handle = dlmopen(LM_ID_NEWLM, lib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    } break;
    case EdcLibraryLoadMethod::DLOPEN: {
        // dlopen places the library in the default memory namespace.
        // - this may have collision with Batsim's memory (e.g., batprotocol-cpp)
        // - this is strongly discouraged if several EDCs should be loaded
        _lib_handle = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    } break;
    }

    xbt_assert(_lib_handle != NULL, "dlmopen failed while loading external decision component library: %s", dlerror());

    init = (uint8_t (*)(const uint8_t*, uint32_t, uint32_t)) load_lib_symbol(_lib_handle, "batsim_edc_init");
    deinit = (uint8_t (*)()) load_lib_symbol(_lib_handle, "batsim_edc_deinit");
    take_decisions = (uint8_t (*)(const uint8_t*, uint32_t, uint8_t**, uint32_t*)) load_lib_symbol(_lib_handle, "batsim_edc_take_decisions");

    XBT_INFO("loaded external decision component library from '%s'", lib_path.c_str());
}

/**
 * @brief Deallocate an ExternalLibrary.
 */
ExternalLibrary::~ExternalLibrary()
{
    deinit();
    dlclose(_lib_handle);
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

/**
 * @brief Calls take_decisions on the ExternalLibrary
 * @param[in] what_happened_buffer The input buffer
 * @param[in] what_happened_buffer_size The input buffer size
 * @param[out] decisions_buffer The output buffer
 * @param[out] decisions_buffer_size The output buffer size
 */
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

/**
 * @brief Builds an ExternalProcess
 * @details Creates a ZMQ socket and connect to the desired endpoint
 * @param[in,out] zmq_context The ZeroMQ context
 * @param[in] connection_endpoint The endpoint onto which the socket should connect
 */
ExternalProcess::ExternalProcess(void *zmq_context, const std::string & connection_endpoint)
{
    // Create the socket
    _zmq_socket = zmq_socket(zmq_context, ZMQ_REQ);
    xbt_assert(_zmq_socket != nullptr, "Cannot create ZMQ_REQ socket (errno=%s)", strerror(errno));

    // Connect to the desired endpoint
    int err = zmq_connect(_zmq_socket, connection_endpoint.c_str());
    xbt_assert(err == 0, "Cannot connect ZMQ socket to '%s' (errno=%s)", connection_endpoint.c_str(), strerror(errno));
}

/**
 * @brief Destroys an ExternalProcess
 * @details The associated ZMQ socket is closed in the process
 */
ExternalProcess::~ExternalProcess()
{
    zmq_close(_zmq_socket);
    _zmq_socket = nullptr;
}

/**
 * @brief Calls take_decisions on the ExternalProcess
 * @param[in] what_happened_buffer The input buffer
 * @param[in] what_happened_buffer_size The input buffer size
 * @param[out] decisions_buffer The output buffer
 * @param[out] decisions_buffer_size The output buffer size
 */
void ExternalProcess::call_take_decisions(uint8_t *what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t **decisions_buffer, uint32_t *decisions_buffer_size)
{
    // Send the message on the socket
    if (zmq_send(_zmq_socket, what_happened_buffer, what_happened_buffer_size, 0) == -1)
        throw std::runtime_error(std::string("Cannot send message on socket (errno=") + strerror(errno) + ")");

    // Wait & read the reply on the socket
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    if (zmq_msg_recv(&msg, _zmq_socket, 0) == -1)
        throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

    *decisions_buffer = (uint8_t *)zmq_msg_data(&msg);
    *decisions_buffer_size = zmq_msg_size(&msg);
}

/**
 * @brief Builds an ExternalDecisionComponent
 * @param[in] type The type of external decision component
 * @param[in] component The actual component (must have been allocated via new before the call)
 */
ExternalDecisionComponent::ExternalDecisionComponent(EDCType type, void *component) :
    _type(type),
    _component(component)
{}

/**
 * @brief Allocates a new ExternalDecisionComponent of library type and initializes it
 * @param[in] lib_path The path of the library to load as an External Decision Component.
 * @param[in] load_method How the library should be loaded into memory
 * @param[in] init_data The initialization data buffer (can be nullptr)
 * @param[in] init_data_size The size of the initialization data buffer
 * @param[in] init_flags The initialization flags
 * @return The newly allocated ExternalDecisionComponent
 */
ExternalDecisionComponent *ExternalDecisionComponent::new_library(
    const std::string & lib_path,
    const EdcLibraryLoadMethod & load_method,
    const uint8_t * init_data,
    uint32_t init_data_size,
    uint32_t init_flags)
{
    auto component = new ExternalLibrary(lib_path, load_method);
    component->init(init_data, init_data_size, init_flags);

    return new ExternalDecisionComponent(EDCType::LIBRARY, static_cast<void*>(component));
}

/**
 * @brief Allocates a new ExternalDecisionComponent of process type and connects it to the desired endpoint
 * @param[in,out] zmq_context The ZeroMQ context
 * @param[in] connection_endpoint The endpoint onto which the socket should connect
 * @return The newly allocated ExternalDecisionComponent
 */
ExternalDecisionComponent *ExternalDecisionComponent::new_process(void *zmq_context, const std::string &connection_endpoint)
{
    auto component = new ExternalProcess(zmq_context, connection_endpoint);

    return new ExternalDecisionComponent(EDCType::PROCESS, static_cast<void*>(component));
}

/**
 * @brief Destroys an ExternalDecisionComponent and the underlying final component
 */
ExternalDecisionComponent::~ExternalDecisionComponent()
{
    switch(_type)
    {
    case EDCType::LIBRARY: {
        delete static_cast<ExternalLibrary*>(_component);
    } break;
    case EDCType::PROCESS: {
        delete static_cast<ExternalProcess*>(_component);
    } break;
    }

    _component = nullptr;
}

/**
 * @brief Calls take_decisions on an ExternalDecisionComponent
 * @param[in] what_happened_buffer The input buffer
 * @param[in] what_happened_buffer_size The input buffer size
 * @param[out] decisions_buffer The output buffer
 * @param[out] decisions_buffer_size The output buffer size
 */
void ExternalDecisionComponent::call_take_decisions(uint8_t *what_happened_buffer, uint32_t what_happened_buffer_size, uint8_t **decisions_buffer, uint32_t *decisions_buffer_size)
{
    switch(_type)
    {
    case EDCType::LIBRARY: {
        static_cast<ExternalLibrary*>(_component)->call_take_decisions(what_happened_buffer, what_happened_buffer_size, decisions_buffer, decisions_buffer_size);
    } break;
    case EDCType::PROCESS: {
        static_cast<ExternalProcess*>(_component)->call_take_decisions(what_happened_buffer, what_happened_buffer_size, decisions_buffer, decisions_buffer_size);
    } break;
    }
}

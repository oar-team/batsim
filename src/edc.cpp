#include "edc.hpp"

#include <dlfcn.h>
#include <string.h>

#include <stdexcept>

#include <zmq.h>

#include <batprotocol.hpp>

#include "protocol.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(edc, "edc"); //!< Logging

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
    const EdcLibraryLoadMethod & load_method)
{
    auto edc = new ExternalDecisionComponent();
    edc->_type = EDCType::LIBRARY;
    edc->_library = new ExternalLibrary();

    switch (load_method)
    {
    case EdcLibraryLoadMethod::DLMOPEN: {
        // dlmopen(LM_ID_NEWLM) places the library in a new memory namespace just for it.
        // this makes sure that the library and all their dependencies are:
        // - loaded into memory, which would not be done if similar libraries existed in the default (batsim's) namespace.
        // - loaded from the desired path/at the desired version if specified in the loaded ELF (e.g., via DT_RUNPATH).
        // - privatized, that is to say that their global variables are not shared between different components.
        edc->_library->lib_handle = dlmopen(LM_ID_NEWLM, lib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    } break;
    case EdcLibraryLoadMethod::DLOPEN: {
        // dlopen places the library in the default memory namespace.
        // - this may have collision with Batsim's memory (e.g., batprotocol-cpp)
        // - this is strongly discouraged if several EDCs should be loaded
        edc->_library->lib_handle = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    } break;
    }

    xbt_assert(edc->_library->lib_handle != NULL, "dynamic loader failed while loading external decision component library: %s", dlerror());

    edc->_library->init = (uint8_t (*)(const uint8_t*, uint32_t, uint32_t*, uint8_t**, uint32_t*)) load_lib_symbol(edc->_library->lib_handle, "batsim_edc_init");
    edc->_library->deinit = (uint8_t (*)()) load_lib_symbol(edc->_library->lib_handle, "batsim_edc_deinit");
    edc->_library->take_decisions = (uint8_t (*)(const uint8_t*, uint32_t, uint8_t**, uint32_t*)) load_lib_symbol(edc->_library->lib_handle, "batsim_edc_take_decisions");

    XBT_INFO("loaded external decision component library from '%s'", lib_path.c_str());

    return edc;
}

/**
 * @brief Allocates a new ExternalDecisionComponent of process type and connects it to the desired endpoint
 * @param[in,out] zmq_context The ZeroMQ context
 * @param[in] connection_endpoint The endpoint onto which the socket should connect
 * @return The newly allocated ExternalDecisionComponent
 */
ExternalDecisionComponent *ExternalDecisionComponent::new_process(void *zmq_context, const std::string &connection_endpoint)
{
    auto edc = new ExternalDecisionComponent();
    edc->_type = EDCType::PROCESS;
    edc->_process = new ExternalProcess();

    // Create the socket
    edc->_process->zmq_socket = zmq_socket(zmq_context, ZMQ_REQ);
    xbt_assert(edc->_process->zmq_socket != nullptr, "Cannot create ZMQ_REQ socket (errno=%s)", strerror(errno));

    // Connect to the desired endpoint
    int err = zmq_connect(edc->_process->zmq_socket, connection_endpoint.c_str());
    xbt_assert(err == 0, "Cannot connect ZMQ socket to '%s' (errno=%s)", connection_endpoint.c_str(), strerror(errno));

    return edc;
}

// Allocate a copy of zmq_data guaranteed to be NULL-terminated
static inline void copy_zmq_buffer_with_null(
    /* input parameters */
    const void * const zmq_data, const size_t zmq_size,
    /* output parameters */
    uint8_t * & copy_data, uint32_t & copy_size)
{
    copy_size = zmq_size + 1;
    copy_data = new uint8_t[copy_size];
    memcpy(copy_data, zmq_data, zmq_size);
    copy_data[zmq_size] = '\0';  // enforce NULL termination
}

/**
 * @brief Call init on the external decision component
 * @param[in] init_data The initialization data
 * @param[in] init_size The initialization data size (in bytes)
 * @param[out] flags The initialization flags, as set by the EDC
 * @param[out] now The timestamp of the EDC hello (reply) message
 * @param[out] messages The inter-actor message list storing the decisions
 * @param[inout] context Batsim context
 */
void ExternalDecisionComponent::init(const uint8_t *init_data, uint32_t init_size, uint32_t & flags, double & now, std::shared_ptr<std::vector<IPMessageWithTimestamp>> & messages, BatsimContext * context)
{
    static_assert(sizeof(init_size) == 4, "batprotocol requires init_size to be encoded with 4 bytes");
    static_assert(sizeof(flags) == 4, "batprotocol requires flags to be encoded with 4 bytes");

    uint8_t * hello_buffer = nullptr;
    uint32_t hello_buffer_size = 0u;

    switch(_type)
    {
    case EDCType::LIBRARY: {
        uint8_t return_code = 0u;
        try {
            return_code = _library->init(init_data, init_size, &flags, &hello_buffer, &hello_buffer_size);
        }
        catch (const std::exception & e) {
            throw std::runtime_error("Exception thrown by the EDC library init function: " + std::string(e.what()));
        }
        if (return_code != 0) {
            throw std::runtime_error("Error while calling init on the EDC library: returned " + std::to_string(return_code));
        }
    } break;

    case EDCType::PROCESS: {
        // TODO: use multipart message instead of sequencing/serializing data by hand.

        // Serialize (in binary) the initialization message (integers sent in native endianness)
        // Format: init_size(u32), init_data(init_size bytes)
        size_t msg_size = sizeof(init_size) + init_size;
        zmq_msg_t init_msg;
        int rc = zmq_msg_init_size(&init_msg, msg_size);
        xbt_assert(rc == 0, "Cannot initialize ZeroMQ message");

        // Fill the message
        uint8_t * buffer_ptr = static_cast<uint8_t *>(zmq_msg_data(&init_msg));
        memcpy(buffer_ptr, &init_size, sizeof(init_size));
        buffer_ptr += sizeof(init_size);
        if (init_size > 0)
        {
            memcpy(buffer_ptr, init_data, init_size);
        }

        // Send the message on the socket
        rc = zmq_msg_send(&init_msg, _process->zmq_socket, 0);
        if (rc != static_cast<int>(msg_size))
        {
            throw std::runtime_error(std::string("Cannot send initialization message on socket (errno=") + strerror(errno) + ")");
        }

        // Wait & read the reply on the socket
        // format: flags(uint32), serialized Message that should contain an EDCHello event
        zmq_msg_t edc_hello_msg;
        zmq_msg_init(&edc_hello_msg);
        if (zmq_msg_recv(&edc_hello_msg, _process->zmq_socket, 0) == -1)
            throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

        uint8_t * raw_zmq_buffer = (uint8_t *)zmq_msg_data(&edc_hello_msg);
        uint32_t raw_zmq_buffer_size = zmq_msg_size(&edc_hello_msg);
        if (raw_zmq_buffer_size < sizeof(flags))
        {
            throw std::runtime_error(std::string("An EDC replied an invalid message to the init sequence: received message should contain at least ") + std::to_string(sizeof(flags)) + " bytes for the serialization flags");
        }

        // Extract flags from raw buffer
        flags = *((uint32_t*)raw_zmq_buffer);

        // Extract the serialized message (the remainder of edc_hello_msg)
        // Parsing expects a NULL-terminated string: use a temporary buffer
        copy_zmq_buffer_with_null(raw_zmq_buffer + sizeof(flags), raw_zmq_buffer_size - sizeof(flags), hello_buffer, hello_buffer_size);

        // Release zmq internal buffer
        zmq_msg_close(&edc_hello_msg);
    } break;
    }

    context->edc_json_format = ((flags & BATSIM_EDC_FORMAT_JSON) != 0);

    if (context->edc_json_format)
    {
        XBT_INFO("Received '%s'", (char *) hello_buffer);
    }

    // Parse EDCHello message and store it in an inter-actor message list
    protocol::parse_batprotocol_message(hello_buffer, hello_buffer_size, now, messages, context);

    if (_type == EDCType::PROCESS) {
        // Release temporary buffer
        delete[] hello_buffer;
        hello_buffer = nullptr;
        hello_buffer_size = 0;
    }
}

/**
 * @brief Destroys an ExternalDecisionComponent and the underlying final component
 */
ExternalDecisionComponent::~ExternalDecisionComponent()
{
    switch(_type)
    {
    case EDCType::LIBRARY: {
        if (_library != nullptr)
        {
            _library->deinit();
            dlclose(_library->lib_handle);
            delete _library;
            _library = nullptr;
        }
    } break;
    case EDCType::PROCESS: {
        zmq_close(_process->zmq_socket);
        _process->zmq_socket = nullptr;
        delete _process;
        _process = nullptr;
    } break;
    }
}

/**
 * @brief Calls take_decisions on an ExternalDecisionComponent
 * @param[in] what_happened_buffer The input buffer
 * @param[in] what_happened_buffer_size The input buffer size
 * @param[out] now The timestamp of the EDC decisions (reply) message
 * @param[out] messages The inter-actor message list storing the decisions
 * @param[in] context Batsim context
 */
void ExternalDecisionComponent::take_decisions(uint8_t * what_happened_buffer, uint32_t what_happened_buffer_size, double & now, std::shared_ptr<std::vector<IPMessageWithTimestamp>> & messages, BatsimContext * context)
{
    uint8_t * decisions_buffer = nullptr;
    uint32_t decisions_buffer_size = 0u;

    if (context->edc_json_format)
    {
        XBT_INFO("Sending '%s'", (const char*) what_happened_buffer);
    }

    switch(_type)
    {
    case EDCType::LIBRARY: {
        uint8_t return_code = 0u;
        try
        {
            XBT_DEBUG("Calling the external library");
            return_code = _library->take_decisions(what_happened_buffer, what_happened_buffer_size, &decisions_buffer, &decisions_buffer_size);
            XBT_DEBUG("External library call finished");
        }
        catch (const std::exception & e)
        {
            throw std::runtime_error("Exception thrown by the EDC library take_decisions function: " + std::string(e.what()));
        }
        if (return_code != 0)
        {
            throw std::runtime_error("Error while calling take_decisions on the EDC library: returned " + std::to_string(return_code));
        }
    } break;

    case EDCType::PROCESS: {
        // Send the message on the socket
        if (zmq_send(_process->zmq_socket, what_happened_buffer, what_happened_buffer_size, 0) == -1)
            throw std::runtime_error(std::string("Cannot send message on socket (errno=") + strerror(errno) + ")");

        // Wait & read the reply on the socket
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        if (zmq_msg_recv(&msg, _process->zmq_socket, 0) == -1)
            throw std::runtime_error(std::string("Cannot read message on socket (errno=") + strerror(errno) + ")");

        // Parsing expects a NULL-terminated string: use a temporary buffer
        copy_zmq_buffer_with_null(zmq_msg_data(&msg), zmq_msg_size(&msg), decisions_buffer, decisions_buffer_size);

        // Release zmq internal buffer
        zmq_msg_close(&msg);
    } break;
    }

    if (context->edc_json_format)
    {
        XBT_INFO("Received '%s'", (char *)decisions_buffer);
    }

    // Parse the EDC decisions and store them in an inter-actor message list
    protocol::parse_batprotocol_message(decisions_buffer, decisions_buffer_size, now, messages, context);

    if (_type == EDCType::PROCESS) {
        // Release temporary buffer
        delete[] decisions_buffer;
        decisions_buffer = nullptr;
        decisions_buffer_size = 0;
    }
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

void edc_decisions_injector(std::shared_ptr<std::vector<IPMessageWithTimestamp> > messages, double now)
{
    for (const auto & message : *messages.get())
    {
        send_message_at_time("server", message.message, message.timestamp);
    }

    send_message_at_time("server", IPMessageType::SCHED_READY, nullptr, now);
}

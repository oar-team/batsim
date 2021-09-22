#pragma once
#include <cstdint>
#include <string>

#include "batsim.hpp"

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
};

void * load_lib_symbol(void * lib_handle, const char * symbol);

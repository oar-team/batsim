#pragma once

#include <string>

namespace roles {
    /**
     * @brief Represents the different permission that can be attributed to each
     * machine though a Role
     */
    enum Permissions : int
    {
        NONE = 0x0,
        COMPUTE_FLOPS = 0x1,
        RECEIVE_BYTES = 0x2,
        SEND_BYTES = 0x4,
        ALTER_STATE = 0x8,
        // Roles
        MASTER = NONE,
        COMPUTE_NODE = COMPUTE_FLOPS | RECEIVE_BYTES | SEND_BYTES,
        STORAGE = RECEIVE_BYTES | SEND_BYTES
    };

    Permissions permissions_from_role(const std::string & str);
}

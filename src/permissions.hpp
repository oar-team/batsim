#pragma once

namespace roles {
/**
 * @brief Represents the different permission that can be attributed to each
 * machine though a Role
 */
enum class Permissions : int
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

const static std::map<std::string, Permissions> map_str_to_role = {
    {"master", Permissions::MASTER},
    {"compute_node", Permissions::COMPUTE_NODE},
    {"storage", Permissions::STORAGE}
};

inline Permissions permissions_from_role(const std::string& str)
{
    if (str == "")
    {
        return Permissions::COMPUTE_NODE;
    }

    try
    {
        return map_str_to_role.at(str);
    }
    catch (std::out_of_range)
    {
        std::string keys_str;
        for (auto const& element : map_str_to_role) {
          keys_str += element.first + ", ";
        }
        throw std::invalid_argument("Cannot create any role from string description '" + str + "' existing roles are: " + keys_str );
    }
};

/**
 * @brief Operator add permissions
 */
inline constexpr Permissions operator|(const Permissions & a, const Permissions & b)
{
    return static_cast<Permissions>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
};

/**
 * @brief Operator to add permission to an existing permission
 */
inline Permissions & operator|=(Permissions & a, Permissions b)
{
    a = a | b;
    return a;
};

/**
 * @brief Operator to filter the permission
 */
inline constexpr Permissions operator&(const Permissions & a, const Permissions & b)
{
    return static_cast<Permissions>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
};

/**
 * @brief Operator to add permission to an existing permission
 */
inline Permissions & operator&=(Permissions & a, Permissions b)
{
    a = a & b;
    return a;
};

/**
 * @brief Operator to compare permissions
 */
inline bool operator==(Permissions a, Permissions b)
{
    return static_cast<unsigned int>(a) == static_cast<unsigned int>(b);
};

};

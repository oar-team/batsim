#include "permissions.hpp"

#include <map>

const static std::map<std::string, roles::Permissions> map_str_to_role = {
    {"master", roles::Permissions::MASTER},
    {"compute_node", roles::Permissions::COMPUTE_NODE},
    {"storage", roles::Permissions::STORAGE}
};

roles::Permissions roles::permissions_from_role(const std::string   & str)
{
    if (str == "")
    {
        return Permissions::COMPUTE_NODE;
    }

    try
    {
        return map_str_to_role.at(str);
    }
    catch (std::out_of_range &)
    {
        std::string keys_str;
        for (auto const& element : map_str_to_role) {
          keys_str += element.first + ", ";
        }
        throw std::invalid_argument("Cannot create any role from string description '" + str + "' existing roles are: " + keys_str );
    }
}

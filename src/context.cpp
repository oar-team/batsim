#include "context.hpp"

/**
 * @brief Destroy a BatsimContext object
 */
BatsimContext::~BatsimContext()
{
    for (auto it : event_lists)
    {
        delete it.second;
    }
}

#include "context.hpp"

BatsimContext::~BatsimContext()
{
    for (auto it : external_event_lists)
    {
        delete it.second;
    }
}

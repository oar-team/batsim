#include "context.hpp"

BatsimContext::~BatsimContext()
{
    for (auto it : event_lists)
    {
        delete it.second;
    }
}

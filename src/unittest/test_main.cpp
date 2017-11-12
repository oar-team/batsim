#include "test_main.hpp"

#include "test_numeric_strcmp.hpp"
#include "test_buffered_outputting.hpp"

void test_entry_point()
{
    test_numeric_strcmp();
    test_pstate_writer();
}

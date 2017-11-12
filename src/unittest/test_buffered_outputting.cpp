#include "test_buffered_outputting.hpp"

#include <stdio.h>

#include "../export.hpp"

void test_buffered_writer()
{
    const char * filename = "/tmp/test_wbuf";
    WriteBuffer * buf = new WriteBuffer(filename, 4);

    // Smaller than the buffer size
    for (int i = 0; i < 10; ++i)
    {
        buf->append_text("ok\n");
    }

    // Exactly the buffer size
    for (int i = 0; i < 10; ++i)
    {
        buf->append_text("meh\n");
    }

    // Bigger than the buffer size
    for (int i = 0; i < 10; ++i)
    {
        buf->append_text("Too big?\n");
    }

    // Flush content, close file and release memory
    delete buf;

    // Remove temporary file
    int remove_ret = remove(filename);
    xbt_assert(remove_ret == 0, "Could not remove file '%s'.", filename);
}

void test_pstate_writer()
{
    const char * filename = "/tmp/test_pstate";
    PStateChangeTracer * tracer = new PStateChangeTracer;
    tracer->setFilename(filename);

    MachineRange range;
    range.insert(0);

    // One machine
    tracer->add_pstate_change(0, range, 0);

    // More machines
    for (int i = 2; i < 100; i+=2)
        range.insert(i);
    tracer->add_pstate_change(1, range, 1);

    // Even more machines
    for (int i = 100; i < 1000; i+=2)
        range.insert(i);
    tracer->add_pstate_change(2, range, 3);

    // Flush content, close file and release memory
    delete tracer;

    // Remove temporary file
    int remove_ret = remove(filename);
    xbt_assert(remove_ret == 0, "Could not remove file '%s'.", filename);
}

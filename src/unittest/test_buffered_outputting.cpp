#include <gtest/gtest.h>

#include <stdio.h>

#include <intervalset.hpp>

#include "../export.hpp"

TEST(buffered_outputting, write_buffer)
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
    EXPECT_EQ(remove_ret, 0) << "Could not remove file " << filename;
}


TEST(buffered_outputting, pstate_writer)
{
    const char * filename = "/tmp/test_pstate";
    PStateChangeTracer * tracer = new PStateChangeTracer;
    tracer->setFilename(filename);

    IntervalSet range;

    // One machine
    range.insert(0);
    tracer->add_pstate_change(0, range, 0);

    // More machines
    for (int i = 2; i < 100; i+=2)
    {
        range.insert(i);
    }
    tracer->add_pstate_change(1, range, 1);

    // Even more machines
    for (int i = 100; i < 1000; i+=2)
    {
        range.insert(i);
    }
    tracer->add_pstate_change(2, range, 3);

    // Flush content, close file and release memory
    delete tracer;

    // Remove temporary file
    int remove_ret = remove(filename);
    EXPECT_EQ(remove_ret, 0) << "Could not remove file " << filename;
}

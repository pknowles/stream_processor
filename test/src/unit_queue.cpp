/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <psp/stream_queue.hpp>

#include <gtest/gtest.h>

using namespace psp;

TEST(Queue, SinglePushPop) {
    stream_queue<int> queue;
    auto writer = queue.make_writer();
    writer.push(1);
    EXPECT_EQ(queue.size(), 1);
    std::optional<int> value = queue.pop();
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(static_cast<bool>(value));
    EXPECT_EQ(*value, 1);
}

TEST(Queue, LastWriterUnblocks) {
    stream_queue<int> queue;
    {
        auto writer = queue.make_writer();
        writer.push(1);
    }
    std::optional<int> value;
    EXPECT_EQ(queue.size(), 1);
    value = queue.pop();
    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(*value, 1);
    value = queue.pop();
    EXPECT_FALSE(static_cast<bool>(value));
}

TEST(Queue, ComplexSkip) {
    stream_queue<int> queue;
    auto it = queue.begin();
    {
        auto writer = queue.make_writer();
        writer.push(1);
        EXPECT_FALSE(it == queue.end()); // the comparison operator reads a value
        EXPECT_EQ(*it, 1); // the value is 1
        EXPECT_EQ(*it, 1); // re-reading it does not fetch another
        writer.push(2);
        writer.push(3);
        EXPECT_EQ(*it, 1); // still the same
        EXPECT_EQ(queue.size(), 2);
        ++it; // skip over 2
        EXPECT_EQ(queue.size(), 2); // lazy iterator hasn't consumed the value yet
        ++it;
        EXPECT_EQ(queue.size(), 1); // lazy again until consuming in ==
        EXPECT_EQ(*it, 3);
        EXPECT_EQ(queue.size(), 0); // now there are no values left
        ++it;
    }

    // End should not block because there are no writers
    EXPECT_EQ(it, queue.end());
}

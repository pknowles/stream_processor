/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <psp/stream_queue.hpp>

#include <gtest/gtest.h>

using namespace psp;

TEST(SinglePushPop, Queue) {
    stream_queue<int> queue;
    auto writer = queue.make_writer();
    writer.push(1);
    EXPECT_EQ(queue.size(), 1);
    std::optional<int> value = queue.pop();
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(static_cast<bool>(value));

    // gtest EXPECT_EQ requires a const equality operator. However, The iterator
    // equality operator cannot be const, because it is lazy and may need to
    // read a value from the queue. Use EXPECT_TRUE instead of EXPECT_EQ.
    EXPECT_TRUE(*value == 1);
}

TEST(LastWriterUnblocks, Queue) {
    stream_queue<int> queue;
    {
        auto writer = queue.make_writer();
        writer.push(1);
    }
    std::optional<int> value;
    EXPECT_EQ(queue.size(), 1);
    value = queue.pop();
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(*value == 1);
    value = queue.pop();
    EXPECT_FALSE(static_cast<bool>(value));
}



TEST(ComplexSkip, Queue) {
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
        EXPECT_TRUE(*it == 1); // still the same
        EXPECT_EQ(queue.size(), 2);
        ++it; // skip over 2
        EXPECT_EQ(queue.size(), 2); // lazy iterator hasn't consumed the value yet
        ++it;
        EXPECT_EQ(queue.size(), 1); // lazy again until consuming in ==
        EXPECT_TRUE(*it == 3);
        EXPECT_EQ(queue.size(), 0); // now there are no values left
        ++it;
    }

    // End should not block because there are no writers
    EXPECT_TRUE(it == queue.end());
}
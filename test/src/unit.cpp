/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <gtest/gtest.h>
#include <stdio.h>

#include <psp/stream_processor.hpp>

using namespace psp;

TEST(BasicSquaresExample, Functional) {
    std::vector<int> input{1, 2, 3};
    parallel_streams squares(input, [](int i){ return i * i; });
    std::vector<int> result(squares.begin(), squares.end());
    ASSERT_EQ(result.size(), input.size());
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 9);
}

TEST(SingleOpSerial, Functional) {
    std::vector<int> thingsToDo;
    for (int i = 0; i < 9; ++i)
        thingsToDo.push_back(i);
    auto increment = [](int item) -> int { return item + 1; };
    parallel_streams runner(thingsToDo, increment, 1);
    int sum = 0;
    for (auto &item : runner)
        sum += item;
    EXPECT_EQ(sum, 45);
}

TEST(SingleOpParallel, Functional) {
    std::vector<int> thingsToDo;
    for (int i = 0; i < 1000; ++i)
        thingsToDo.push_back(i);
    auto increment = [](int item) -> int { return item + 1; };
    parallel_streams runner(thingsToDo, increment);
    int sum = 0;
    for (auto &item : runner)
        sum += item;
    EXPECT_EQ(sum, 500500);
}

TEST(DoubleOpParallel, Functional) {
    std::vector<int> thingsToDo;
    for (int i = 0; i < 10; ++i)
        thingsToDo.push_back(i);
    auto increment = [](int item) -> int { return item + 1; };
    auto decrement = [](int item) -> int { return item - 1; };
    parallel_streams runner1(thingsToDo, increment);

    // TODO: should be able to avoid this stall!
    std::vector<int> delete_me(runner1.begin(), runner1.end());

    parallel_streams runner2(delete_me, decrement);
    int sum = 0;
    for (auto &item : runner2)
        sum += item;
    EXPECT_EQ(sum, 45);
}

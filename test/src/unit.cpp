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
    parallel_streams squares(input.begin(), input.end(), [](int i){ return i * i; });
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
    parallel_streams runner(thingsToDo.begin(), thingsToDo.end(), increment, 1);
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
    parallel_streams runner(thingsToDo.begin(), thingsToDo.end(), increment);
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
    parallel_streams runner1(thingsToDo.begin(), thingsToDo.end(), increment);
    parallel_streams runner2(runner1.begin(), runner1.end(), decrement);
    int sum = 0;
    for (auto &item : runner2)
        sum += item;
    EXPECT_EQ(sum, 45);
}

TEST(StressPipeline, Functional) {
    std::vector<int> input;
    for (int i = 1; i < 1000; ++i)
        input.push_back(i);
    auto collatz = [](int x) -> int {
        if (x <= 1)
            return 0;
        return (x & 1) ? 3 * x + 1 : x / 2;
    };
    // Casually create 178 threads
    parallel_streams first(input.begin(), input.end(), collatz);
    using Processor = parallel_streams<decltype(first)::iterator, decltype(collatz)>;
    std::vector<std::unique_ptr<Processor>> processors;
    for (int i = 0; i < 177; ++i)
        processors.push_back(std::make_unique<Processor>(
            i == 0 ? first.begin() : processors.back()->begin(),
            i == 0 ? first.end() : processors.back()->end(), collatz, 1));
    int sum = 0;
    int i = 0;
    // https://en.wikipedia.org/wiki/Collatz_conjecture
    // "less than 1000 is 871, which has 178 steps,"
    for (auto &item : *processors.back())
    {
        if (++i == 871)
            EXPECT_EQ(item, 1);
        sum += item;
    }
    EXPECT_EQ(sum, 1);
}

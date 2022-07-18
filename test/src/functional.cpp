/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <psp/stream_processor.hpp>
#include <psp/thread_pool.hpp>

#include <condition_variable>
#include <gtest/gtest.h>
#include <list>
#include <mutex>
#include <stdio.h>
#include <thread>

using namespace psp;

// Helper class to block a wait()ing thread until something calls step()
class stepper {
public:
    void wait() {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_cond.wait(lk, [&] { return m_waits < m_steps; });
        ++m_waits;
    }
    void step() {
        std::lock_guard<std::mutex> lk(m_mutex);
        ++m_steps;
        m_cond.notify_one();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    size_t m_waits{0};
    size_t m_steps{0};
};

TEST(Functiona, BasicSquaresExample) {
    std::vector<int> input{1, 2, 3};
    parallel_streams squares(input.begin(), input.end(),
                             [](int i) { return i * i; });
    std::vector<int> result(squares.begin(), squares.end());
    ASSERT_EQ(result.size(), input.size());
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 9);
}

TEST(Functional, SingleOpSerial) {
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

TEST(Functional, SingleOpParallel) {
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

TEST(Functional, DoubleOpParallel) {
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

TEST(Functional, StressPipelineLockstep) {
    const int inputsCount = 1000;
    const int pipelineLength = 178;
    struct Item {
        int startValue;
        int value;
        int step;
        int stoppingTime;
    };
    stepper lockstep;
    std::vector<Item> input;
    for (int i = 1; i < inputsCount; ++i)
        input.push_back({i, i, 0, 0});
    std::atomic<int> pipelineTopIndex{0};
    auto collatz = [&pipelineTopIndex](Item item) -> Item {
        if (item.step == 0)
        {
            // every processor only has one thread so there should be no way for
            // them to get out of order
            int expectedStartValue = ++pipelineTopIndex;
            EXPECT_EQ(expectedStartValue, item.startValue);
        }
        item.step++;
        if (item.value == 1)
            item.value = 0;
        item.value = (item.value & 1) ? 3 * item.value + 1 : item.value / 2;
        if (item.value == 1)
            item.stoppingTime = item.step;
        return item;
    };
    auto collatzStep = [&lockstep, &collatz](Item item) -> Item {
        lockstep.wait();
        return collatz(item);
    };
    // Casually create 178 threads. Expect data comes out in the same order
    // because there's no way for a thread to skip ahead.
    parallel_streams first(input.begin(), input.end(), collatzStep, 1);
    using Processor =
        parallel_streams<decltype(first)::iterator, decltype(collatz)>;
    std::vector<std::unique_ptr<Processor>> processors;
    for (int i = 0; i < pipelineLength - 1; ++i)
        processors.push_back(std::make_unique<Processor>(
            i == 0 ? first.begin() : processors.back()->begin(),
            i == 0 ? first.end() : processors.back()->end(), collatz, 1));
    int sum = 0;
    int i = 0;
    lockstep.step();
    for (auto &item : *processors.back()) {
        lockstep.step();
        EXPECT_EQ(item.startValue, i + 1);
        EXPECT_EQ(item.step, pipelineLength);

        // https://en.wikipedia.org/wiki/Collatz_conjecture
        // "less than 1000 is 871, which has 178 steps,"
        if (item.startValue == 871)
            EXPECT_EQ(item.stoppingTime, 178);

        sum += item.value;
        i++;
    }
    EXPECT_EQ(sum, 1);
}

TEST(Functional, StressPipeline) {
    std::vector<int> input;
    for (int i = 1; i < 1000; ++i)
        input.push_back(i);
    auto collatz = [](int x) -> int {
        if (x <= 1)
            return 0;
        return (x & 1) ? 3 * x + 1 : x / 2;
    };
    // Casually create 2x178 threads
    parallel_streams first(input.begin(), input.end(), collatz, 2);
    using Processor =
        parallel_streams<decltype(first)::iterator, decltype(collatz)>;
    std::vector<std::unique_ptr<Processor>> processors;
    for (int i = 0; i < 177; ++i)
        processors.push_back(std::make_unique<Processor>(
            i == 0 ? first.begin() : processors.back()->begin(),
            i == 0 ? first.end() : processors.back()->end(), collatz, 2));
    int sum = 0;
    int i = 0;
    // https://en.wikipedia.org/wiki/Collatz_conjecture
    // "less than 1000 is 871, which has 178 steps,"
    for (auto &item : *processors.back()) {
        sum += item;
    }
    EXPECT_EQ(sum, 1);
}

TEST(Functional, ThreadPoolLockstep) {
    std::vector<int> thingsToDo;
    for (int i = 0; i < 10; ++i)
        thingsToDo.push_back(i);

    // Use the stepper to verify the pipeline can complete if only one thing can
    // be processed at a time
    stepper lockstep;

    auto increment = [&](int item) -> int {
        lockstep.wait();
        return item + 1;
    };
    auto decrement = [](int item) -> int { return item - 1; };
    stream_processor proc1(thingsToDo.begin(), thingsToDo.end(), increment);
    stream_processor proc2(proc1.begin(), proc1.end(), decrement);

    // Manual thread pool that is shared by both processor objects
    thread_pool threads(2);
    threads.process(proc1.make_processor());
    threads.process(proc2.make_processor());

    // Prime the loops, releasing a single item to be processed
    lockstep.step();

    int sum = 0;
    for (auto &item : proc2) {
        EXPECT_EQ(proc1.size(), 0);
        EXPECT_EQ(proc2.size(), 0);
        lockstep.step();
        sum += item;
    }
    EXPECT_EQ(sum, 45);
}

TEST(Functional, DifferentTypes) {
    std::vector<int> input{1, 2, 3};
    std::set<std::string> expected{"1", "4", "9"};
    thread_pool threads;
    parallel_streams squareInts(
        input.begin(), input.end(), [](int i) { return i * i; }, threads);
    parallel_streams squareStrings(
        squareInts.begin(), squareInts.end(),
        [](int i) { return std::to_string(i); }, threads);
    std::set<std::string> result(squareStrings.begin(), squareStrings.end());
    EXPECT_EQ(result, expected);
}
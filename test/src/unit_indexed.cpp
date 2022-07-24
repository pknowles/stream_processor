/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <gtest/gtest.h>
#include <psp/indexed_processing.hpp>
#include <psp/stream_processor.hpp>

TEST(IndexedIterator, IteratorBasic) {
    std::vector<int> ints{0, 1, 2, 3};
    psp::indexed_iterator<std::vector<int>::iterator> begin(ints.begin());
    psp::indexed_iterator<std::vector<int>::iterator> end(ints.begin());
    for (; begin != end; ++begin)
    {
        auto& value = *begin;
        EXPECT_EQ(value.index, value.value);
        EXPECT_EQ(value.step, 0);
    }
}

TEST(IndexedIterator, FunctionEndToEnd) {
    std::vector<int> ints{0, 1, 2, 3};

    // Wrap the input with indexed iterators
    psp::indexed_iterator<std::vector<int>::iterator> begin(ints.begin());
    psp::indexed_iterator<std::vector<int>::iterator> end(ints.begin());

    // A simple function that takes an index and pipeline step
    auto intToString = [](size_t index, size_t step, int i) {
        EXPECT_EQ(index, i);
        EXPECT_EQ(step, 0);
        return std::to_string(i);
    };

    // Wrap the function to automatically update index and step
    psp::indexed_function wrap(intToString);

    // Process the items, with horrible automagic enumeration
    psp::parallel_streams processor(begin, end, wrap, 2);
    for (auto &result : processor) {
        EXPECT_EQ(std::to_string(result.index), result.value);
        EXPECT_EQ(result.step, 1);
    }
}

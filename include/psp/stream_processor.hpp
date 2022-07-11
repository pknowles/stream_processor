/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <assert.h>
#include <stdio.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>

#include "function_traits.hpp"
#include "stream_queue.hpp"

namespace psp {

template <typename> struct is_tuple : std::false_type {};
template <typename... T> struct is_tuple<std::tuple<T...>> : std::true_type {};

template <class InputIterator, class Func> class stream_processor {
public:
    using input_value_type = typename InputIterator::value_type;
    using function_arg0_type = typename function_traits<Func>::arg_type;
    using output_value_type = typename function_traits<Func>::return_type;

    stream_processor(InputIterator begin, InputIterator end,
                     stream_queue<output_value_type> &output, const Func &func)
        : m_inputBegin(begin), m_inputEnd(end), m_output(output), m_func(func) {
    }

    void process() {
        auto writer = m_output.make_writer();
        input_value_type item;
        while (getOneInput(item)) {
            // NOTE: TOTALLY UNTESTED!
            // Automatically expand inputs of tuples to function arguments,
            // unless the function intends to take a tuple as the first argument
            if constexpr (is_tuple<input_value_type>() &&
                          !is_tuple<function_arg0_type>())
                writer->push(std::apply(m_func, item));
            else
                writer->push(m_func(item));
        }
    }

private:
    bool getOneInput(input_value_type &item) {
        std::lock_guard<std::mutex> lk(m_inputMutex);
        bool hasItem = m_inputBegin != m_inputEnd;
        if (hasItem) {
            // TODO: is this safe? it is desirable to move from
            // consuming_queue_iterator, but only because iterators will never
            // hit the same value
            if constexpr (std::is_same_v<
                              typename InputIterator::iterator_category,
                              std::input_iterator_tag>)
                item = std::move(*m_inputBegin);
            else
                item = *m_inputBegin;
            ++m_inputBegin;
        }
        return hasItem;
    }

    Func m_func;
    InputIterator m_inputBegin;
    InputIterator m_inputEnd;
    std::mutex m_inputMutex;
    stream_queue<output_value_type> &m_output;
};

template <class Container, class Func>
using container_to_stream_processor =
    stream_processor<typename Container::iterator, Func>;

/**
 * \brief stream_processor convenience wrapper
 *
 * Includes a stream_processor that takes input from a given container, using
 * its begin()/end(). Automatically starts threads to do the processing. Uses
 * stream_queue for output, which also supports iteration with begin()/end().
 * Items are produced in the order processing finishes.
 *
 * Example:
 * @code
 * std::vector<int> input{1, 2, 3};
 * parallel_streams squares(input, [](int i){ return i * i; });
 * for (auto &item : squares)
 *     std::cout << item << std::endl;
 * @endcode
 */
template <class Container, class Func>
class parallel_streams
    : public container_to_stream_processor<Container, Func>,
      public stream_queue<typename function_traits<Func>::return_type> {
public:
    parallel_streams(Container &container, const Func &func,
                     size_t thread_count = std::thread::hardware_concurrency())
        : container_to_stream_processor<Container, Func>(
              std::begin(container), std::end(container), *this, func) {
        start(thread_count);
    }
    ~parallel_streams() {
        for (auto &thread : m_threads)
            thread.join();
    }

    using stream_queue<typename function_traits<Func>::return_type>::begin;
    using stream_queue<typename function_traits<Func>::return_type>::end;

private:
    void start(size_t thread_count) {
        m_threads.reserve(thread_count);
        for (size_t i = 0; i < thread_count; ++i)
            m_threads.emplace_back(
                &container_to_stream_processor<Container, Func>::process,
                (container_to_stream_processor<Container, Func> *)this);
    }
    std::vector<std::thread> m_threads;
};

} // namespace psp

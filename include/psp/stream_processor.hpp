/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <assert.h>
#include <stdio.h>

#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>

#include "function_traits.hpp"
#include "stream_queue.hpp"

// TODO: split to another file, along with parallel_streams
#include "thread_pool.hpp"

namespace psp {

template <typename> struct is_tuple : std::false_type {};
template <typename... T> struct is_tuple<std::tuple<T...>> : std::true_type {};

template <class InputIterator, class Func> class iterable_processor {
public:
    using input_value_type = typename InputIterator::value_type;
    using function_arg0_type = std::tuple_element_t<0, typename function_traits<Func>::arg_types>;
    using output_value_type = typename function_traits<Func>::return_type;

    iterable_processor(InputIterator begin, InputIterator end,
                       stream_queue<output_value_type> &output,
                       const Func &func)
        : m_inputBegin(begin), m_inputEnd(end), m_output(output), m_func(func) {
    }

    void process_all() {
        auto p = make_processor();
        while (p())
            ;
    }

    std::function<bool()> make_processor() {
        auto writer = m_output.make_writer();
        return [this, writer]() mutable -> bool {
            auto item = getOneInput();
            bool hasItem = item.has_value();
            if (hasItem) {
                // NOTE: TOTALLY UNTESTED!
                // Automatically expand inputs of tuples to function arguments,
                // unless the function intends to take a tuple as the first
                // argument
                if constexpr (is_tuple<input_value_type>() &&
                              !is_tuple<function_arg0_type>())
                    writer.push(std::apply(m_func, *item));
                else
                    writer.push(m_func(*item));
            }
            return hasItem;
        };
    }

private:
    std::optional<input_value_type> getOneInput() {
        std::lock_guard<std::mutex> lk(m_inputMutex);
        bool hasItem = m_inputBegin != m_inputEnd;
        if (hasItem) {
            // TODO: is this safe? it is desirable to move from
            // consuming_queue_iterator, but only because iterators will never
            // hit the same value
            if constexpr (std::is_same_v<
                              typename InputIterator::iterator_category,
                              std::input_iterator_tag>)
                return std::move(*m_inputBegin++);
            else
                return *m_inputBegin++;
        }
        return {};
    }

    Func m_func;
    InputIterator m_inputBegin;
    InputIterator m_inputEnd;
    std::mutex m_inputMutex;
    stream_queue<output_value_type> &m_output;
};

template <class InputIterator, class Func>
class stream_processor
    : public iterable_processor<InputIterator, Func>,
      public stream_queue<typename function_traits<Func>::return_type> {
public:
    stream_processor(InputIterator begin, InputIterator end, const Func &func)
        : iterable_processor<InputIterator, Func>(begin, end, *this, func) {}
};

/**
 * \brief stream_processor with threads
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
template <class InputIterator, class Func>
class parallel_streams : public stream_processor<InputIterator, Func> {
public:
    // Constructor with own dedicated threads
    parallel_streams(InputIterator begin, InputIterator end, const Func &func,
                     size_t thread_count = std::thread::hardware_concurrency())
        : stream_processor<InputIterator, Func>(begin, end, func) {
        start(thread_count);
    }

    // Constructor to use a shared thread pool
    parallel_streams(InputIterator begin, InputIterator end, const Func &func,
                     thread_pool &threads)
        : stream_processor<InputIterator, Func>(begin, end, func) {
        threads.process(
            stream_processor<InputIterator, Func>::make_processor());
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
                &stream_processor<InputIterator, Func>::process_all,
                (stream_processor<InputIterator, Func> *)this);
    }
    std::vector<std::thread> m_threads;
};

} // namespace psp

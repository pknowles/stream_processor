/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "stream_queue.hpp"
#include "function_traits.hpp"

#include <assert.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdio.h>
#include <thread>
#include <type_traits>

namespace psp {

template <class Func> class stream_processor {
public:
  using input_value = typename function_traits<Func>::arg_type;
  using output_value = typename function_traits<Func>::return_type;
  using output_iterator = typename stream_queue<output_value>::iterator;

  stream_processor(const Func &func) : m_func(func) {}

  template <class T> void push(T &&value) {
    m_input.promise(1);
    m_output.promise(1);
    m_input.push(std::make_tuple(std::forward<T>(value)));
  }

  // connect multiple streams and avoid a stall where one to use push_from
  template <class OtherFunc>
  void take_from(stream_processor<OtherFunc> &other) {
    printf("Push from queue\n");
    m_input.promise(other.size());
    m_output.promise(other.size());
    other.m_outputNext = &m_input;
  }

  template <class Container> void push_from(Container &&container) {
    printf("Push from container\n");
    m_input.promise(container.size());
    m_output.promise(container.size());
    m_input.push_from(std::forward<Container>(container));
  }

  output_iterator begin() { return m_output.begin(); }
  output_iterator end() { return m_output.end(); }
  output_value pop() { return m_output.pop(); }
  std::size_t size() const { return m_output.size(); }

  void process() {
    stream_queue<output_value> *output =
        m_outputNext ? m_outputNext : &m_output;
    std::optional<input_value> args;
    // TODO: use std::apply(m_func, args) when args are a tuple
    while ((args = m_input.try_pop()))
      output->push(m_func(*args));
  }

private:
  Func m_func;
  stream_queue<input_value> m_input;
  stream_queue<output_value> m_output;

public:
  stream_queue<output_value> *m_outputNext{};
};

template <class Func> class parallel_streams : public stream_processor<Func> {
public:
  parallel_streams(const Func &func) : stream_processor<Func>(func) {}

  void start(size_t thread_count = std::thread::hardware_concurrency()) {
    m_threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i)
      m_threads.emplace_back(&stream_processor<Func>::process,
                             (stream_processor<Func> *)this);
  }
  ~parallel_streams() {
    for (auto &thread : m_threads)
      thread.join();
  }

private:
  std::vector<std::thread> m_threads;
};

}

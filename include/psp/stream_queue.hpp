/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

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

template <class T> class stream_queue {
public:
  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::size_t;
    using value_type = T;
    using pointer = value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;

    iterator(stream_queue<value_type> &queue, std::size_t position)
        : m_queue(queue), m_position(position) {}
    const_reference operator*() const { read(); return *m_value; }
    reference operator*() { read(); return *m_value; }
    const pointer operator->() const { read(); return &m_value; }
    pointer operator->() { read(); return &m_value; }
    difference_type operator-(iterator &other) {
      return m_position - other.m_position;
    }
    iterator &operator++() {
      // Make sure to consume the value, e.g. when iterating without reading.
      read();
      m_value.reset();
      ++m_position;
      return *this;
    }
    iterator operator++(int) {
      iterator tmp(*this);
      ++(*this);
      return tmp;
    }
    bool operator==(const iterator &other) const {
      return m_position == other.m_position;
    };
    bool operator!=(const iterator &other) const {
      return m_position != other.m_position;
    };

  private:
    void read()
    {
        if (!m_value)
            m_value = m_queue.iterator_pop();
    }
    stream_queue<value_type> &m_queue;
    std::size_t m_position;
    std::optional<value_type> m_value;
  };

  void promise(size_t count) {
    std::lock_guard<std::mutex> lk(m_mutex);
    // Must promise everything up-front, before reading
    assert(!m_consuming);
    m_promised += count;
  }

  template <class V> void push(V &&value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.push(std::forward<V>(value));
    m_cond.notify_all();
  }

  template <typename Container> void push_from(Container &container) {
    // TODO: push in batches if the container is huge? maybe pass the iterator
    // to the consumer?
    std::lock_guard<std::mutex> lk(m_mutex);
    for (T &value : container)
      m_queue.push(value);
    m_cond.notify_all();
  }

  // Wait for, pop and return an element
  T pop() {
    std::unique_lock<std::mutex> lk(m_mutex);
    consume_locked(1);
    return pop_locked(lk);
  }

  // Wait for, pop and return an element if there are still promised items
  std::optional<T> try_pop() {
    std::optional<T> result;
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_consumed < m_promised) {
      consume_locked(1);
      result = pop_locked(lk);
    }
    return result;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_promised - m_consumed;
  }
  iterator begin() {
    std::unique_lock<std::mutex> lk(m_mutex);
    std::size_t tmp = m_consumed;
    consume_locked(m_promised - m_consumed);
    return iterator(*this, tmp);
  }
  iterator end() { return iterator(*this, m_promised); }

private:
  // Pop an element without incrementing m_consumed as it has been already
  // allocated to an iterator
  T iterator_pop() {
    std::unique_lock<std::mutex> lk(m_mutex);
    return pop_locked(lk);
  }

  // Not exception safe and involves a copy
  T pop_locked(std::unique_lock<std::mutex> &lk) {
    m_consuming = true;
    m_cond.wait(lk, [&] { return !m_queue.empty(); });
    T value = std::move(m_queue.front());
    m_queue.pop();
    return value;
  }

  void consume_locked(size_t items) {
    m_consumed += items;

    // Allow reuse after consuming all items, i.e. more promises
    if (m_consumed == m_promised)
      m_consuming = false;
  }

  std::condition_variable m_cond;
  mutable std::mutex m_mutex;
  std::queue<T> m_queue;
  std::size_t m_consumed{0};
  std::size_t m_promised{0};
  bool m_consuming{false};
};

}

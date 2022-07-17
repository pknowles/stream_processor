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
#include <thread>
#include <type_traits>

namespace psp {

template <class Queue> class consuming_queue_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::size_t;
    using value_type = typename Queue::value_type;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;

    consuming_queue_iterator(Queue &queue, bool end)
        : m_queue(queue), m_end(end) {}
    const_reference operator*() const {
        read();
        return m_value.value();
    }
    reference operator*() {
        read();
        return m_value.value();
    }
    const_pointer operator->() const {
        read();
        return &m_value.value();
    }
    pointer operator->() {
        read();
        return &m_value.value();
    }
    difference_type operator-(consuming_queue_iterator &other) {
        if (m_end && !other.m_end)
            return other.m_queue.size();
        assert(!other.m_end); // would need a negative difference
        return 0; // non-end iterators are all implicitly at the same position
    }
    consuming_queue_iterator &operator++() {
        // Consume the value in case of iterating without reading
        read();
        m_value.reset();
        return *this;
    }
    consuming_queue_iterator operator++(int) {
        consuming_queue_iterator tmp(*this);
        ++(*this);
        return tmp;
    }
    bool operator==(consuming_queue_iterator &other) {
        read();
        other.read();
        return m_value.has_value() == other.m_value.has_value();
    };
    bool operator!=(consuming_queue_iterator &other) {
        return !(*this == other);
    };

private:
    void read() {
        if (!m_end && !m_value.has_value())
            m_value = m_queue.pop();
    }
    Queue &m_queue;
    std::size_t m_position;
    std::optional<value_type> m_value;
    bool m_end;
};

template <class T> class stream_queue {
public:
    using value_type = T;
    using iterator = consuming_queue_iterator<stream_queue>;

    class writer {
    public:
        writer(stream_queue &queue) : m_queue(&queue) { m_queue->writer_open(); }
        ~writer() { if (m_queue) m_queue->writer_close(); }

        // Copy constructor - must open a new reference
        writer(const writer &other) : m_queue(other.m_queue) { m_queue->writer_open(); }

        // Move constructor - must stop the other queue from closing its reference
        writer(writer &&other) : m_queue(std::move(other.m_queue)) { other.m_queue = nullptr; }

        writer &operator=(const writer &other) = delete;

        template <class V> void push(V &&value) {
            m_queue->push(std::forward<V>(value));
        }

    private:
        stream_queue *m_queue;
    };

    std::optional<value_type> pop() {
        std::optional<value_type> result;
        std::unique_lock<std::mutex> lk(m_mutex);
        m_cond.wait(lk, [&] { return !m_writers || !m_queue.empty(); });
        if (!m_queue.empty()) {
            result = std::move(m_queue.front());
            m_queue.pop();
        } else
            assert(!m_writers);
        return result;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_queue.size();
    }

    iterator begin() { return iterator(*this, false); }
    iterator end() { return iterator(*this, true); }

    writer make_writer() {
        auto result = writer(*this);
        if (!m_hasFirstWriter) {
            // Remove the internal refcount so that readers will be notified
            // when the last writer is deleted.
            writer_close();
            m_hasFirstWriter = true;
        }
        return result;
    }

private:
    // Only accessible to writers
    void writer_open() {
        std::lock_guard<std::mutex> lk(m_mutex);
        ++m_writers;
    }

    // Only accessible to writers
    void writer_close() {
        std::lock_guard<std::mutex> lk(m_mutex);
        assert(m_writers > 0);
        if (--m_writers == 0)
            m_cond.notify_all();
    }

    // Only accessible to writers
    template <class V> void push(V &&value) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_queue.push(std::forward<V>(value));
        m_cond.notify_all();
    }

    std::condition_variable m_cond;
    mutable std::mutex m_mutex;
    std::queue<value_type> m_queue;

    // Refcount the number of writers, so the readers know when the stream has
    // finished. The alternative would be to promise a number of items that will
    // be written. Initialize with a single refcount to force readers to wait
    // for at least one writer.
    uint32_t m_writers{1};
    bool m_hasFirstWriter{false};
};

} // namespace psp

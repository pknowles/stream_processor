/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <assert.h>
#include <condition_variable>
#include <mutex>
#include <list>
#include <thread>
#include <functional>

namespace psp {

class thread_pool
{
public:
    using multitask = std::function<bool()>;

    thread_pool(size_t count)
    {
        // All threads start from the beginning of the queue and do one pass through it
        
        for (size_t i = 0; i < count; ++i)
            m_threads.emplace_back(&thread_pool::entrypoint, this, m_multitasks.begin());
    }
    ~thread_pool()
    {
        {
            std::lock_guard<std::mutex> lk(m_multitasksMutex);
            m_running = false;
            m_multitasksCond.notify_all();
        }
        for (auto& thread : m_threads)
            thread.join();
    }

    template<class Func>
    void process(Func&& func)
    {
        std::lock_guard<std::mutex> lk(m_multitasksMutex);
        m_multitasks.emplace_back(func);
        m_multitasksCond.notify_all();
    }

private:
    void entrypoint(std::list<multitask>::iterator iterator) {
        bool first = true;
        bool taskDone = false;
        for (;;) {
            std::unique_lock<std::mutex> lk(m_multitasksMutex);
            if (!first) {
                if (taskDone) {
                    m_multitasks.erase(iterator++);
                } else {
                    iterator++;
                }
            } else {
                first = false;
            }
            if (iterator == m_multitasks.end()) {
                m_multitasksCond.wait(
                    lk, [&] { return !m_running || !m_multitasks.empty(); });
                if (!m_running)
                    return;
                iterator = m_multitasks.begin();
            }
            auto &task = *iterator;
            taskDone = !task();
        }
    }

    std::list<multitask> m_multitasks;
    std::vector<std::thread> m_threads;
    std::mutex m_multitasksMutex;
    std::condition_variable m_multitasksCond;
    bool m_running{true};
};

} // namespace psp

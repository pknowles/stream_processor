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
#include <list>
#include <mutex>
#include <thread>

namespace psp {

class thread_pool {
public:
    using multitask = std::function<bool()>;

    thread_pool(size_t count) {
        // All threads start from the beginning of the queue and do one pass
        // through it

        for (size_t i = 0; i < count; ++i)
            m_threads.emplace_back(&thread_pool::entrypoint, this,
                                   m_multitasks.begin());
    }
    ~thread_pool() {
        {
            std::lock_guard<std::mutex> lk(m_multitasksMutex);
            m_running = false;
            m_multitasksCond.notify_all();
        }
        for (auto &thread : m_threads)
            thread.join();
    }

    template <class Func> void process(Func &&func) {
        std::lock_guard<std::mutex> lk(m_multitasksMutex);
        m_tasksAlive++;
        m_multitasks.emplace_back(func);
        m_multitasksCond.notify_all();
    }

private:
    struct Task {
        std::shared_ptr<multitask> func;
        bool alive{true};
        Task() {}
        template <class Func>
        Task(Func &&func) : func(std::make_shared<multitask>(func)) {}
    };

    void entrypoint(std::list<Task>::iterator iterator) {
        bool first = true;
        bool taskDone = false;
        Task task;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(m_multitasksMutex);
                if (!first) {
                    if (taskDone && iterator->alive) {
                        iterator->alive = false;
                        --m_tasksAlive;
                    }
                    if (!iterator->alive && task.func.use_count() == 2) {
                        // The last user of finished tasks removes it.
                        // There "thould" (*hopes even harder*) be no dangling
                        // iterators
                        m_multitasks.erase(iterator++);
                    } else {
                        iterator++;
                    }
                } else {
                    first = false;
                }

                // Release our reference to the task so the last thread can
                // remove it from the list.
                task.func.reset();

                // Skip over completed tasks. Each of them should have threads
                // waiting on this lock to come in and erase them.
                while (iterator != m_multitasks.end() && !iterator->alive)
                    ++iterator;

                if (iterator == m_multitasks.end()) {
                    m_multitasksCond.wait(
                        lk, [&] { return !m_running || m_tasksAlive; });
                    if (!m_running)
                        return;
                    iterator = m_multitasks.begin();
                    while (!iterator->alive)
                        ++iterator;
                }
                task = *iterator;
            }
            taskDone = !(*task.func)();
        }
    }

    std::list<Task> m_multitasks;
    std::vector<std::thread> m_threads;
    std::mutex m_multitasksMutex;
    std::condition_variable m_multitasksCond;
    bool m_running{true};
    size_t m_tasksAlive{0};
};

} // namespace psp

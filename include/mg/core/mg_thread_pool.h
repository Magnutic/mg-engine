//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//	claim that you wrote the original software. If you use this software
//	in a product, an acknowledgement in the product documentation would be
//	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_thread_pool.h
 * Thread pool implementation
 */

// Inspired by github.com/nbsdx/ThreadPool and github.com/progschj/ThreadPool

#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "mg/utils/mg_assert.h"

namespace Mg {

/** Simple thread pool. Creates the given number of worker threads on
 * construction, using them to run jobs added via ThreadPool::add_job.
 * Jobs are run as soon as a worker thread is available, if none are then the
 * jobs will be queued. */
class ThreadPool {
public:
    /** Construct a new ThreadPool
     * @param thread_count How many threads the pool should have. */
    explicit ThreadPool(size_t thread_count);

    /** All jobs will be finished before destruction. */
    ~ThreadPool();

    /** Get the number of threads in this ThreadPool. */
    size_t size() const { return m_threads.size(); }

    /** Add function call as job to the pool.
     * @return std::future for the return value of the function */
    template<typename Fn, typename... Args>
    auto add_function(Fn&&, Args&&...) -> std::future<std::result_of_t<Fn(Args...)>>;

    /** Add job to the pool. */
    template<typename Fn, typename... Args> void add_job(Fn&&, Args&&...);

    /** Wait for all jobs in the pool to finish, locking the thread. */
    void await_all_jobs();

private:
    /** Run by worker threads. Loops forever, acquiring and executing available
     * jobs until ThreadPool is destroyed */
    void execute_job_loop();

private:
    std::vector<std::thread> m_threads;        // Worker threads
    std::queue<std::function<void()>> m_queue; // Job queue

    bool m_exiting = false; // Whether ThreadPool is being destroyed
    size_t m_num_jobs = 0;  // Number of unstarted + started and unfinished jobs

    // Worker threads without jobs await this condition variable
    std::condition_variable m_job_available_var;

    // await_all_jobs() waits on this condition var to check if all jobs are done
    std::condition_variable m_wait_var;

    // Mutex locked before accessing jobs queue and related metadata
    std::mutex m_jobs_mutex;
};

//--------------------------------------------------------------------------------------------------
// ThreadPool implementation
//--------------------------------------------------------------------------------------------------

ThreadPool::ThreadPool(size_t thread_count)
{
    for (size_t i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this] { this->execute_job_loop(); });
    }
}

ThreadPool::~ThreadPool()
{
    // Wait for all added jobs to finish
    await_all_jobs();

    {
        std::lock_guard<std::mutex> lock{ m_jobs_mutex };
        m_exiting = true; // Allow threads to break their loop
    }

    // Wake pool threads that are waiting for jobs
    m_job_available_var.notify_all();

    // Sanity check
    MG_ASSERT_DEBUG(m_num_jobs == 0);
    MG_ASSERT_DEBUG(m_queue.size() == 0);

    // Join all threads
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template<typename Fn, typename... Args>
auto ThreadPool::add_function(Fn&& fn, Args&&... args) -> std::future<std::result_of_t<Fn(Args...)>>
{
    using RetType = std::result_of_t<Fn(Args...)>;

    auto task = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

    std::future<RetType> ret_val = task->get_future();

    std::lock_guard<std::mutex> lock{ m_jobs_mutex };

    m_queue.emplace([task] { (*task)(); });
    ++m_num_jobs;
    m_job_available_var.notify_one();

    return ret_val;
}

template<typename Fn, typename... Args> void ThreadPool::add_job(Fn&& fn, Args&&... args)
{
    std::lock_guard<std::mutex> lock{ m_jobs_mutex };
    m_queue.push(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
    ++m_num_jobs;
    m_job_available_var.notify_one();
}

void ThreadPool::await_all_jobs()
{
    std::unique_lock<std::mutex> lock{ m_jobs_mutex };
    if (m_num_jobs > 0) {
        m_wait_var.wait(lock, [this] { return m_num_jobs == 0; });
    }
}

void ThreadPool::execute_job_loop()
{
    for (;;) {
        std::function<void()> current_job;

        {
            std::unique_lock<std::mutex> lock{ m_jobs_mutex };

            // Wait until a job is available (or the pool is being destroyed)
            m_job_available_var.wait(lock, [this] { return m_queue.size() > 0 || m_exiting; });

            if (m_exiting) {
                return;
            }

            current_job = m_queue.front();
            m_queue.pop();
        }

        // Execute the current job
        current_job();

        {
            std::lock_guard<std::mutex> lock{ m_jobs_mutex };
            --m_num_jobs;
        }

        // Notify main thread in case it is waiting for all jobs to finish
        m_wait_var.notify_one();
    }
}

} // namespace Mg

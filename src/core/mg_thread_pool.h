//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_thread_pool.h
 * Thread pool implementation
 */

// Inspired by github.com/nbsdx/ThreadPool and github.com/progschj/ThreadPool

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/utils/mg_assert.h"

#include <function2/function2.hpp>

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

namespace Mg {

/** Simple thread pool. Creates the given number of worker threads on
 * construction, using them to run jobs added via ThreadPool::add_job.
 * Jobs are run as soon as a worker thread is available, if none are then the
 * jobs will be queued.
 */
class ThreadPool {
public:
    /** Construct a new ThreadPool
     * @param thread_count How many threads the pool should have.
     */
    explicit ThreadPool(size_t thread_count);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /** All jobs will be finished before destruction. */
    ~ThreadPool();

    /** Get the number of threads in this ThreadPool. */
    size_t size() const { return m_threads.size(); }

    /** Add function (object) as job to the pool.
     * @remark: To pass arguments into the function, write a lambda as the job and capture the
     * arguments. Make sure to by-value-capture arguments with short lifetimes!
     * @return std::future for the return value of the function.
     */
    [[nodiscard]] auto add_job(std::invocable auto job)
        -> std::future<std::invoke_result_t<decltype(job)>>;

    /** Add function (object) as job to the pool.
     * @remark: To pass arguments into the function, write a lambda as the job and capture the
     * arguments. Make sure to by-value-capture arguments with short lifetimes!
     */
    void add_job_fire_and_forget(std::invocable auto job) { enqueue_job(std::move(job)); }

    /** Invoke func on all elements in range, distributing the invocations into multiple parallel
     * jobs.
     */
    auto parallel_for(std::ranges::contiguous_range auto& range,
                      size_t num_elems_per_job,
                      const std::invocable<decltype(*std::ranges::data(range))> auto& func);

    /** Wait for all jobs in the pool to finish, locking the thread. */
    void await_all_jobs();

    /** Get number of jobs currently running or waiting to run. */
    size_t num_jobs() const
    {
        std::lock_guard guard{ m_jobs_mutex };
        return m_num_jobs;
    }

private:
    // Run by worker threads. Loops forever, acquiring and executing available
    // jobs until ThreadPool is destroyed.
    void execute_job_loop();

    void enqueue_job(fu2::unique_function<void()> job_wrapper);

private:
    std::vector<std::thread> m_threads;               // Worker threads
    std::queue<fu2::unique_function<void()>> m_queue; // Job queue

    bool m_exiting = false; // Whether ThreadPool is being destroyed
    size_t m_num_jobs = 0;  // Number of unstarted + started and unfinished jobs

    // Worker threads without jobs await this condition variable
    std::condition_variable m_job_available_var;

    // await_all_jobs() waits on this condition var to check if all jobs are done
    std::condition_variable m_wait_var;

    // Mutex locked before accessing jobs queue and related metadata
    mutable std::mutex m_jobs_mutex;
};

//--------------------------------------------------------------------------------------------------
// ThreadPool implementation
//--------------------------------------------------------------------------------------------------

inline ThreadPool::ThreadPool(size_t thread_count)
{
    for (size_t i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this] { this->execute_job_loop(); });
    }
}

inline ThreadPool::~ThreadPool()
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
    MG_ASSERT_DEBUG(m_queue.empty());

    // Join all threads
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

inline void ThreadPool::enqueue_job(fu2::unique_function<void()> job_wrapper)
{
    std::lock_guard<std::mutex> lock{ m_jobs_mutex };
    m_queue.emplace(std::move(job_wrapper));
    ++m_num_jobs;
    m_job_available_var.notify_one();
}

auto ThreadPool::add_job(std::invocable auto job)
    -> std::future<std::invoke_result_t<decltype(job)>>
{
    using JobReturnT = std::invoke_result_t<decltype(job)>;

    auto packaged_task = std::packaged_task<JobReturnT()>(std::move(job));
    std::future<JobReturnT> ret_val = packaged_task.get_future();
    enqueue_job(std::move(packaged_task));
    return ret_val;
}

auto ThreadPool::parallel_for(std::ranges::contiguous_range auto& range,
                              const size_t num_elems_per_job,
                              const std::invocable<decltype(*std::ranges::data(range))> auto& func)
{
    const auto span = std::span(range);
    const size_t num_elems = span.size();

    const size_t num_jobs_to_spawn = num_elems_per_job < num_elems
                                         ? (num_elems + num_elems_per_job - size_t(1)) /
                                               num_elems_per_job
                                         : size_t(1);
    std::atomic_size_t num_done = 0;

    for (size_t i = 1; i < num_jobs_to_spawn; ++i) {
        const auto start = i * num_elems_per_job;
        const auto num = std::min(start + num_elems_per_job, num_elems) - start;
        add_job_fire_and_forget([&num_done, subrange = span.subspan(start, num), func] {
            for (auto& v : subrange) {
                func(v);
            }
            ++num_done;
        });
    }

    // Do some work on this thread, too.
    const auto num = std::min(num_elems_per_job, num_elems);
    for (auto&& v : span.subspan(0, num)) {
        func(v);
    }
    ++num_done;

    // Wait until all jobs are done.
    while (num_done < num_jobs_to_spawn) {
        std::this_thread::yield();
    }
}


inline void ThreadPool::await_all_jobs()
{
    std::unique_lock<std::mutex> lock{ m_jobs_mutex };
    if (m_num_jobs > 0) {
        m_wait_var.wait(lock, [this] { return m_num_jobs == 0; });
    }
}

inline void ThreadPool::execute_job_loop()
{
    for (;;) {
        fu2::unique_function<void()> current_job;

        {
            std::unique_lock<std::mutex> lock{ m_jobs_mutex };

            // Wait until a job is available (or the pool is being destroyed)
            m_job_available_var.wait(lock, [this] { return !m_queue.empty() || m_exiting; });

            if (m_exiting) {
                return;
            }

            current_job = std::move(m_queue.front());
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

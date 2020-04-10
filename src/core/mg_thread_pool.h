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

#include "mg/utils/mg_assert.h"

#include <function2/function2.hpp>

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
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

    /** All jobs will be finished before destruction. */
    ~ThreadPool();

    /** Get the number of threads in this ThreadPool. */
    size_t size() const { return m_threads.size(); }

    // Metafunction for getting appropriate return type for add_job.
    template<typename Job>
    using AddJobReturnT = std::conditional_t<std::is_void_v<std::invoke_result_t<Job>>,
                                             void,
                                             std::future<std::invoke_result_t<Job>>>;

    /** Add function (object) as job to the pool.
     * @remark: To pass arguments into the function, write a lambda as the job and capture the
     * arguments. Make sure to by-value-capture arguments with short lifetimes!
     * @return std::future for the return value of the function, unless the return type is void, in
     * which case add_job also returns also void.
     */
    template<typename Job> auto add_job(Job) -> AddJobReturnT<Job>;

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

void ThreadPool::enqueue_job(fu2::unique_function<void()> job_wrapper)
{
    std::lock_guard<std::mutex> lock{ m_jobs_mutex };
    m_queue.emplace(std::move(job_wrapper));
    ++m_num_jobs;
    m_job_available_var.notify_one();
}

template<typename Job> auto ThreadPool::add_job(Job job) -> AddJobReturnT<Job>
{
    using JobReturnT = std::invoke_result_t<Job>;
    static constexpr bool returns_void = std::is_void_v<JobReturnT>;

    if constexpr (returns_void) {
        enqueue_job(std::move(job));
    }
    else {
        auto packaged_task = std::packaged_task<JobReturnT()>(std::move(job));
        std::future<JobReturnT> ret_val = packaged_task.get_future();
        enqueue_job(std::move(packaged_task));
        return ret_val;
    }
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
        fu2::unique_function<void()> current_job;

        {
            std::unique_lock<std::mutex> lock{ m_jobs_mutex };

            // Wait until a job is available (or the pool is being destroyed)
            m_job_available_var.wait(lock, [this] { return m_queue.size() > 0 || m_exiting; });

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

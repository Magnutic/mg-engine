#include "catch.hpp"

#include <chrono>
#include <cstdio>
#include <random>

// Use Mg::FlatMap just to test it a bit, too.
#include <mg/containers/mg_flat_map.h>

// mg_thread_pool.h is a private header, so we have to include it by explicit path.
#include "../src/core/mg_thread_pool.h"

TEST_CASE("ThreadPool: timing test")
{
    using namespace std::chrono;

    puts("Starting timing_test");

    Mg::ThreadPool pool{ 2 };
    size_t job_count = 2;

    auto start_time = high_resolution_clock::now();

    for (size_t i = 0u; i < job_count; ++i) {
        pool.add_job([] { std::this_thread::sleep_for(milliseconds(100u)); });
    }

    pool.await_all_jobs();

    auto end_time = high_resolution_clock::now();
    auto diff = duration_cast<milliseconds>(end_time - start_time).count();

    // Check that jobs ran in parallel
    // TODO: quite brittle. Is there a better way?
    CHECK(diff >= 90);
    CHECK(diff <= 110);
}

TEST_CASE("ThreadPool: return test")
{
    puts("Starting return_test");
    Mg::ThreadPool pool{ 4 };

    auto future = pool.add_job([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    });

    puts("Launched job");

    REQUIRE(future.get() == true);

    puts("Finished return_test");
}

TEST_CASE("ThreadPool: many jobs")
{
    Mg::ThreadPool pool{ std::thread::hardware_concurrency() };
    Mg::FlatMap<int, int> job_to_expected_result_map;
    Mg::FlatMap<int, std::future<int>> job_to_future_map;

    std::default_random_engine r(123);
    std::uniform_int_distribution<int> dist;

    for (int i = 0; i < 1000; ++i)
    {
        const auto result = dist(r);
        const auto wait_time_us = dist(r) % 10;

        job_to_expected_result_map.insert({i, result});

        auto future = pool.add_job([result, wait_time_us] {
            std::this_thread::sleep_for(std::chrono::microseconds(wait_time_us));
            return result;
        });

        job_to_future_map.insert({ i, std::move(future) });
    }

    for (const auto& [job_index, expected_result] : job_to_expected_result_map)
    {
        std::future<int>& future = job_to_future_map[job_index];
        REQUIRE(future.get() == expected_result);
    }

    // Since we have been get()ing the result from each job's future, all jobs should be done by
    // now.
    REQUIRE(pool.num_jobs() == 0);
}

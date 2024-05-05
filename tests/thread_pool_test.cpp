#include "catch.hpp"

#include <chrono>
#include <cstdio>
#include <random>

// Use Mg::FlatMap just to test it a bit, too.
#include <mg/containers/mg_flat_map.h>

// mg_thread_pool.h is a private header, so we have to include it by explicit path.
#include "../src/core/mg_thread_pool.h"

// Some tests check the time taken to verify that jobs are executed in parallel. Of course, such
// tests are brittle and may fail in some environments. Therefore, they are disabled by default.
#ifndef PERFORM_TIMING_SENSITIVE_TESTS
#define PERFORM_TIMING_SENSITIVE_TESTS 1
#endif

#if PERFORM_TIMING_SENSITIVE_TESTS
TEST_CASE("ThreadPool: timing test")
{
    using namespace std::chrono;

    puts("Starting timing_test");

    Mg::ThreadPool pool{ 2 };
    size_t job_count = 2;

    auto start_time = high_resolution_clock::now();

    for (size_t i = 0u; i < job_count; ++i) {
        pool.add_job_fire_and_forget([] { std::this_thread::sleep_for(milliseconds(100u)); });
    }

    pool.await_all_jobs();
    CHECK(pool.num_jobs() == 0);

    auto end_time = high_resolution_clock::now();
    auto diff = duration_cast<milliseconds>(end_time - start_time).count();

    // Check that jobs ran in parallel
    CHECK(diff >= 90);
    CHECK(diff <= 110);
}
#endif

TEST_CASE("ThreadPool: return test")
{
    puts("Starting return_test");
    Mg::ThreadPool pool{ 4 };

    auto future_a = pool.add_job([] { return true; });
    auto future_b = pool.add_job([] { return false; });

    puts("Launched job");

    REQUIRE(future_a.get() == true);
    REQUIRE(future_b.get() == false);

    puts("Finished return_test");
}

TEST_CASE("ThreadPool: many jobs")
{
    Mg::ThreadPool pool{ std::thread::hardware_concurrency() };
    Mg::FlatMap<int, int> job_to_expected_result_map;
    Mg::FlatMap<int, std::future<int>> job_to_future_map;

    std::default_random_engine r(123);
    std::uniform_int_distribution<int> dist;

    for (int i = 0; i < 1000; ++i) {
        const auto result = dist(r);
        const auto wait_time_us = dist(r) % 10;

        job_to_expected_result_map.insert({ i, result });

        auto future = pool.add_job([result, wait_time_us] {
            std::this_thread::sleep_for(std::chrono::microseconds(wait_time_us));
            return result;
        });

        job_to_future_map.insert({ i, std::move(future) });
    }

    for (const auto& [job_index, expected_result] : job_to_expected_result_map) {
        std::future<int>& future = job_to_future_map[job_index];
        REQUIRE(future.get() == expected_result);
    }
}

TEST_CASE("ThreadPool: parallel_for")
{
    using namespace std::chrono;

    Mg::ThreadPool pool{ std::thread::hardware_concurrency() };

    struct Elem {
        size_t i{};
        std::string s;
    };

    std::vector<Elem> elems;
    elems.resize(1024 * 1024);

    auto init = [&elems] {
        size_t i = 0;
        for (Elem& elem : elems) {
            elem.i = i++;
            elem.s.clear();
        }
    };

    init();

    auto job = [](Elem& elem) { elem.s = std::to_string(elem.i); };

#if PERFORM_TIMING_SENSITIVE_TESTS
    // Warm up
    {
        for (Elem& v : elems) {
            job(v);
        }
        for (Elem& v : elems) {
            job(v);
        }
    }
#endif

    [[maybe_unused]] milliseconds time_parallel{};
    [[maybe_unused]] milliseconds time_sequential{};

#if PERFORM_TIMING_SENSITIVE_TESTS
    {
        init();
        auto start_time = high_resolution_clock::now();
        for (Elem& v : elems) {
            job(v);
        }
        auto end_time = high_resolution_clock::now();
        time_sequential = duration_cast<milliseconds>(end_time - start_time);
        REQUIRE(
            std::ranges::all_of(elems, [](const Elem& v) { return v.s == std::to_string(v.i); }));
    }
#endif

    {
        init();
        auto start_time = high_resolution_clock::now();
        pool.parallel_for(elems, 1000, job);
        auto end_time = high_resolution_clock::now();
        time_parallel = duration_cast<milliseconds>(end_time - start_time);
        REQUIRE(
            std::ranges::all_of(elems, [](const Elem& v) { return v.s == std::to_string(v.i); }));
    }

#if PERFORM_TIMING_SENSITIVE_TESTS
    REQUIRE(time_parallel < time_sequential);
#endif
}

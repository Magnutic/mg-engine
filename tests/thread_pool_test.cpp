#include "catch.hpp"

#include <chrono>
#include <cstdio>

#include <mg/core/mg_thread_pool.h>

// TODO: more correctness tests; thousands of randomised jobs, verify results.

TEST_CASE("ThreadPool: timing test")
{
    using namespace std::chrono;

    puts("Starting timing_test");

    Mg::ThreadPool pool{ 2 };
    size_t job_count = 2;

    auto start_time = high_resolution_clock::now();

    for (auto i = 0u; i < job_count; ++i) {
        pool.add_job([] { std::this_thread::sleep_for(milliseconds(100u)); });
    }

    pool.await_all_jobs();

    auto end_time = high_resolution_clock::now();
    auto diff = duration_cast<milliseconds>(end_time - start_time).count();

    // Check that jobs ran in parallel
    // TODO: quite brittle. Is there a better way?
    CHECK(diff >= 90);
    CHECK(diff <= 110);

    puts("Expected runtime: 2 seconds.");
}

TEST_CASE("ThreadPool: return test")
{
    puts("Starting return_test");
    Mg::ThreadPool pool{ 4 };

    auto future = pool.add_function([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    });

    puts("Launched job");

    REQUIRE(future.get() == true);

    puts("Finished return_test");
}

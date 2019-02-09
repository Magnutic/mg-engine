#include "catch.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>

#include <mg/memory/mg_defragmenting_allocator.h>

// TODO: test const handle, test alloc_copy

struct S {
    int     i{};
    int64_t i64{};
    char    char_buf[32] = "Hello";
};

TEST_CASE("DefragmentingAllocator: basic test")
{
    Mg::memory::DefragmentingAllocator da(32 * sizeof(S));

    Mg::memory::DA_UniquePtr<S[]> sh = da.alloc<S[]>(2);
    for (S& s : sh) { REQUIRE(std::string_view(s.char_buf) == "Hello"); }

    Mg::memory::DA_UniquePtr<char[]> string_h;

    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    Mg::memory::DA_Ptr<char[]> string_p;

    {
        Mg::memory::DA_UniquePtr<S[]> tmp = da.alloc<S[]>(10);
        string_h                          = da.alloc<char[]>(50);
        string_p                          = string_h;
        std::strncpy(string_h.get(), alphabet, 32);
        REQUIRE(std::string_view(string_h.get()) == alphabet);

        Mg::memory::DA_UniquePtr<bool> bool_handle = da.alloc<bool>(true);
        REQUIRE(*bool_handle == true);
        REQUIRE(*Mg::memory::DA_Ptr<const bool>(bool_handle) == true);
    }

    da.defragment();
    REQUIRE(std::string_view(string_h.get()) == "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    REQUIRE(std::string_view(string_p.get()) == "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

TEST_CASE("DefragmentingAllocator: LIFO test")
{
    static constexpr size_t num_allocs = 32;
    using namespace Mg::memory;

    // DefragmentingAllocator has special logic for the case that allocations are deallocated in
    // LIFO order. This allows the space occupied by the allocation to be re-used immediately
    // without requiring defragmentation.

    DefragmentingAllocator da(num_allocs * sizeof(S));

    // Fill allocator
    std::array<DA_UniquePtr<S>, num_allocs> allocs;
    for (size_t i = 0; i < num_allocs; ++i) {
        allocs[i]    = da.alloc<S>();
        allocs[i]->i = int(i);
        std::strncpy(allocs[i]->char_buf, "old allocs", 32);
    }

    REQUIRE(da.free_space() == 0);

    // Remove half in LIFO order
    for (size_t i = 0; i < num_allocs / 2; ++i) {
        auto j = 32 - i - 1;
        allocs[j].reset();
    }

    REQUIRE(da.free_space() == (num_allocs / 2) * sizeof(S));
    REQUIRE(da.has_space_for<S>(num_allocs / 2));

    // Re-fill cleared half with new allocations.
    // If reset() above did not cause the allocator to re-use the deallocated storage, then this
    // loop would through bad_alloc
    for (size_t i = num_allocs / 2; i < num_allocs; ++i) {
        allocs[i]    = da.alloc<S>();
        allocs[i]->i = int(i);
        std::strncpy(allocs[i]->char_buf, "new allocs", 32);
    }

    // Make sure old allocations are unaffected.
    for (size_t i = 0; i < num_allocs / 2; ++i) {
        REQUIRE(allocs[i]->i == int(i));
        REQUIRE(std::string_view(allocs[i]->char_buf) == "old allocs");
    }

    // Make sure new allocations have the right data.
    for (size_t i = num_allocs / 2; i < num_allocs; ++i) {
        REQUIRE(allocs[i]->i == int(i));
        REQUIRE(std::string_view(allocs[i]->char_buf) == "new allocs");
    }

    // Clear whole allocator in LIFO order.
    for (auto it = allocs.rbegin(); it != allocs.rend(); ++it) { it->reset(); }

    REQUIRE(da.free_space() == num_allocs * sizeof(S));
    REQUIRE(da.has_space_for<S>(num_allocs));
}

struct Elem {
    Mg::memory::DA_UniquePtr<uint32_t[]> handle;
    uint32_t                             value;
};

TEST_CASE("DefragmentingAllocator: randomised test")
{
    const size_t k_iterations = 10000;

    const size_t heap_size      = 1024;
    const size_t max_num_allocs = 500;

    std::default_random_engine              re(35872);
    std::uniform_int_distribution<uint32_t> dist;

    Mg::memory::DefragmentingAllocator da(heap_size);
    std::vector<Elem>                  refs;

    auto verify_data = [&refs] {
        for (auto& elem : refs) {
            for (auto& i : elem.handle) { REQUIRE(elem.value == i); }
        }
    };

    using TestIterationFunc = std::function<void(uint32_t)>;

    std::array<TestIterationFunc, 4> test_funcs{ {

        // Alloc new
        [&](uint32_t arg) {
            auto num = arg % 40;
            if (!da.has_space_for<S>(num)) {
                std::cout << "Cannot alloc new, not enough space\n";
                return;
            }
            std::cout << "Alloc new\n";

            auto                     handle = da.alloc<uint32_t[]>(num);
            std::fill(handle.begin(), handle.end(), arg);

            refs.push_back(Elem{ std::move(handle), arg });
        },

        // Delete handle
        [&](uint32_t arg) {
            if (refs.empty()) { return; }
            std::cout << "Delete handle\n";
            refs.erase(refs.begin() + (arg % refs.size()));
        },

        // Shuffle handles (test handle move, swap)
        [&](uint32_t /*arg*/) {
            std::cout << "Shuffle handles\n";
            std::shuffle(refs.begin(), refs.end(), re);
        },

        // Compact
        [&](uint32_t arg) {
            if (arg % 10 == 0) { // Do one time in 10
                std::cout << "Compact\n";
                da.defragment();
            }
        } } };

    for (size_t i = 0; i < k_iterations; ++i) {
        auto num_a = dist(re);
        auto num_b = dist(re);

        test_funcs[num_a % test_funcs.size()](num_b);
        verify_data();
    }

    refs.clear();
}

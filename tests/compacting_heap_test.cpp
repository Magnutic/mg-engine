#include "catch.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>

#include <mg/memory/mg_compacting_heap.h>

// TODO: test const handle, test alloc_copy

struct S {
    int     i{};
    int64_t i64{};
    char    char_buf[32] = "Hello";
};

TEST_CASE("CompactingHeap: basic test")
{
    Mg::memory::CompactingHeap ch(32 * sizeof(S));

    Mg::memory::CHHandle<S[]> sh = ch.alloc<S[]>(2);
    for (S& s : sh) {
        REQUIRE(std::string_view(s.char_buf) == "Hello");
    }

    Mg::memory::CHHandle<char[]> string_h;

    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    Mg::memory::CHPtr<char[]> string_p;

    {
        Mg::memory::CHHandle<S[]> tmp = ch.alloc<S[]>(10);
        string_h                      = ch.alloc<char[]>(50);
        string_p                      = string_h;
        std::strncpy(string_h.get(), alphabet, 50);
        REQUIRE(std::string_view(string_h.get()) == alphabet);

        Mg::memory::CHHandle<bool> bool_handle = ch.alloc<bool>(true);
        REQUIRE(*bool_handle == true);
        REQUIRE(*Mg::memory::CHPtr<const bool>(bool_handle) == true);
    }

    ch.compact();
    REQUIRE(std::string_view(string_h.get()) == "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    REQUIRE(std::string_view(string_p.get()) == "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

struct Elem {
    Mg::memory::CHHandle<uint32_t[]> handle;
    uint32_t                         value;
};

TEST_CASE("CompactingHeap: randomised test")
{
    const size_t k_iterations = 10000;

    const size_t heap_size      = 1024;
    const size_t max_num_allocs = 500;

    std::default_random_engine              re(35872);
    std::uniform_int_distribution<uint32_t> dist;

    Mg::memory::CompactingHeap ch(heap_size);
    std::vector<Elem>          refs;

    auto verify_data = [&refs] {
        for (auto& elem : refs) {
            for (auto& i : elem.handle) {
                REQUIRE(elem.value == i);
            }
        }
    };

    using TestIterationFunc = std::function<void(uint32_t)>;

    std::array<TestIterationFunc, 4> test_funcs{ {

        // Alloc new
        [&](uint32_t arg) {
            auto num = arg % 40;
            if (!ch.has_space_for<S>(num)) {
                std::cout << "Cannot alloc new, not enough space\n";
                return;
            }
            std::cout << "Alloc new\n";

            auto                     handle = ch.alloc<uint32_t[]>(num);
            std::fill(handle.begin(), handle.end(), arg);

            refs.push_back(Elem{ std::move(handle), arg });
        },

        // Delete handle
        [&](uint32_t arg) {
            if (refs.empty()) {
                return;
            }
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
                ch.compact();
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

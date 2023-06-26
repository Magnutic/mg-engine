#include "catch.hpp"

#include <algorithm>
#include <array>
#include <random>
#include <set>
#include <string>

#include <fmt/core.h>

#include <mg/containers/mg_slot_map.h>
#include <mg/utils/mg_instance_counter.h>

using namespace Mg;

// TODO: test swap (both member and free); test SlotMapInserter; test
// growing; test that handles are valid after resize, move, and copy; test exception safety; test
// range-erase

struct Type {
    Type() = default;
    Type(size_t v) : value(v) {}
    ~Type() = default;

    Type(const Type&) = default;
    Type(Type&&) = default; // NOLINT(bugprone-exception-escape)
    Type& operator=(const Type&) = default;
    Type& operator=(Type&&) = default; // NOLINT(bugprone-exception-escape)

    size_t& get_value_checked()
    {
        if (counter.is_moved_from()) {
            throw std::runtime_error("Type::get_value_checked(): is moved-from.");
        }
        if (counter.is_destroyed()) {
            throw std::runtime_error("Type::get_value_checked(): is destroyed.");
        }

        return value;
    }

    size_t value = 0;

    InstanceCounter<Type, false> counter;
};

std::ostream& operator<<(std::ostream& os, const Type& t)
{
    return os << t.value;
}

namespace Mg {

// Make Slot_map printable for error messages
template<typename T> std::ostream& operator<<(std::ostream& os, const Slot_map<T>& smap)
{
    os << "Slot_map{ ";
    if (!smap.empty()) {
        auto last = smap.end();
        --last;
        for (auto it = smap.begin(); it != last; ++it) {
            os << *it << ", ";
        }
        os << *last;
    }
    return os << " }\n";
}

// Make Slot_map comparable for testing
template<typename T> bool operator==(const Slot_map<T>& l, const Slot_map<T>& r)
{
    return l.size() == r.size() && std::equal(l.begin(), l.end(), r.begin());
}

} // namespace Mg

TEST_CASE("Slot_map test")
{
    static constexpr uint32_t smap_size = 25;
    Slot_map<std::string> smap{ smap_size };

    SECTION("can_construct") {}


    SECTION("can_insert")
    {
        for (uint32_t i = 0; i < smap_size; ++i) {
            std::string s = fmt::format("Elem {}", i);
            smap.insert(s);
        }

        REQUIRE(smap.size() == smap_size);
    }


    SECTION("can_move_insert")
    {
        for (uint32_t i = 0; i < smap_size; ++i) {
            std::string s = fmt::format("Elem {}", i);
            smap.insert(std::move(s));
        }
    }


    SECTION("can_emplace")
    {
        for (uint32_t i = 0; i < smap_size; ++i) {
            std::string s = fmt::format("Elem {}", i);
            smap.emplace(s.c_str());
        }

        REQUIRE(smap.size() == smap_size);
    }


    SECTION("can_erase")
    {
        for (uint32_t i = 0; i < smap_size; ++i) {
            std::string s = fmt::format("Elem {}", i);
            smap.insert(s);
        }

        auto elem_count = smap_size;
        REQUIRE(smap.size() == elem_count);

        for (auto it = smap.begin(); it != smap.end();) {
            it = smap.erase(it);
            --elem_count;
        }

        REQUIRE(elem_count == 0);
        REQUIRE(smap.size() == elem_count);
    }

    SECTION("resize")
    {
        smap.insert("Hello");

        smap.resize(1);

        REQUIRE(*smap.begin() == "Hello");

        smap.resize(2 * smap_size);

        REQUIRE(*smap.begin() == "Hello");
        REQUIRE(smap.capacity() == 2 * smap_size);
    }
}

TEST_CASE("Prefilled Slot_map test")
{
    static constexpr size_t smap_size = 25;
    Slot_map<size_t> smap{ smap_size };
    std::array<Slot_map_handle, smap_size> handles;

    for (size_t i = 0; i < smap_size; ++i) {
        auto handle = smap.insert(i);
        handles[i] = handle;
    }

    SECTION("handle")
    {
        // Make sure handles are invalidated when elements are erased
        smap.erase(handles[15]);

        for (auto& h : handles) {
            if (h != handles[15]) {
                REQUIRE(smap.is_handle_valid(h));
            }
            else {
                REQUIRE(!smap.is_handle_valid(h));
            }
        }

        // Check that all handles point to correct data
        handles[15] = smap.insert(15);

        for (size_t i = 0; i < smap_size; ++i) {
            auto& h = handles[i];
            REQUIRE(smap.is_handle_valid(h));
            REQUIRE(smap[h] == i);
        }
    }


    SECTION("contiguous_iteration")
    {
        // Memory layout order iteration (using iterators).
        using value_type = decltype(smap)::value_type;

        std::set<value_type> set;

        for (auto& it : smap) {
            set.insert(it);
        }

        for (value_type i = 0; i < smap_size; ++i) {
            REQUIRE(set.find(i) != set.end());
        }
    }


    SECTION("const_iteration")
    {
        // Iteration should also work with const SlotMaps
        using value_type = decltype(smap)::value_type;

        const auto& csmap = smap;
        std::set<value_type> set;

        for (auto& it : csmap) {
            set.insert(it);
        }

        for (value_type i = 0; i < smap_size; ++i) {
            REQUIRE(set.find(i) != set.end());
        }
    }


    SECTION("reverse_iteration")
    {
        // Test reverse iterators
        auto rit = smap.rbegin();
        auto it = smap.end();
        --it;

        const size_t num_elems = smap.size();
        size_t i = 0u;

        for (; rit != smap.rend(); ++rit, --it, ++i) {
            REQUIRE(*rit == *it);
        }

        REQUIRE(i == num_elems);
    }


    SECTION("const_reverse_iteration")
    {
        // Test const reverse iterators
        auto crit = smap.crbegin();
        auto cit = smap.cend();
        --cit;

        const size_t num_elems = smap.size();
        size_t i = 0u;

        for (; crit != smap.crend(); ++crit, --cit, ++i) {
            REQUIRE(*crit == *cit);
        }

        REQUIRE(i == num_elems);
    }


    SECTION("handle_iteration")
    {
        // Creation order iteration (using handles).
        for (size_t i = 0; i < smap.size(); ++i) {
            auto h = handles[i];
            REQUIRE(smap[h] == i);
        }
    }


    SECTION("const_handle")
    {
        // Make sure const handles are usable
        const auto& csmap = smap;

        const auto ch = handles[0];
        auto const_elem = csmap[ch];

        auto h = handles[0];
        auto elem = smap[h];

        REQUIRE(const_elem == elem);
    }


    SECTION("get_handle_from_iterator")
    {
        for (auto it = smap.begin(); it != smap.end(); ++it) {
            auto handle = smap.make_handle(it);
            REQUIRE(smap[handle] == *it);
        }
    }

    SECTION("InsertionIterator")
    {
        decltype(smap) smap_copy(smap.size());
        std::copy(smap.begin(), smap.end(), slot_map_inserter(smap_copy));
        REQUIRE(smap_copy == smap);
    }
}

TEST_CASE("Counting Slot_map test")
{
    static constexpr uint32_t initial_size = 25;
    Slot_map<Type> smap{ initial_size };

    // Test to check whether elements are correctly constructed and destroyed
    SECTION("counter")
    {
        // No element constructors should have been called at Slot_map construction
        REQUIRE(InstanceCounter<Type>::get_counter() == 0);
        REQUIRE(smap.size() == 0); // NOLINT
        REQUIRE(smap.empty());

        {
            std::array<Type, 5> counters;
            REQUIRE(InstanceCounter<Type>::get_counter() == 5);
            for (auto& elem : counters) {
                smap.insert(elem);
            }

            REQUIRE(InstanceCounter<Type>::get_counter() == 10);
        }

        REQUIRE(InstanceCounter<Type>::get_counter() == 5);
        REQUIRE(InstanceCounter<Type>::get_counter_move() == 5);
        REQUIRE(smap.size() == 5);

        smap.erase(smap.begin());
        smap.erase(smap.begin());

        REQUIRE(InstanceCounter<Type>::get_counter() == 3);
        REQUIRE(smap.size() == 3);
        {
            std::array<Type, 7> counters;
            REQUIRE(InstanceCounter<Type>::get_counter() == 10);
            REQUIRE(InstanceCounter<Type>::get_counter_move() == 10);

            for (auto& elem : counters) {
                smap.insert(std::move(elem));
            }

            REQUIRE(InstanceCounter<Type>::get_counter() == 10);
            REQUIRE(InstanceCounter<Type>::get_counter_move() == 17);
            REQUIRE(smap.size() == 10);
        }

        REQUIRE(InstanceCounter<Type>::get_counter() == 10);
        REQUIRE(InstanceCounter<Type>::get_counter_move() == 10);
        REQUIRE(smap.size() == 10);

        smap.resize(2 * initial_size);
        REQUIRE(InstanceCounter<Type>::get_counter() == 10);
        REQUIRE(InstanceCounter<Type>::get_counter_move() == 10);
        REQUIRE(smap.size() == 10);

        while (!smap.empty()) {
            smap.erase(smap.begin());
        }
        REQUIRE(InstanceCounter<Type>::get_counter() == 0);
        REQUIRE(InstanceCounter<Type>::get_counter_move() == 0);
        REQUIRE(smap.size() == 0); // NOLINT
        REQUIRE(smap.empty());

        // Refill Slot_map to make sure new size is fully usable
        for (size_t i = 0; i < 2 * initial_size; ++i) {
            smap.emplace();
        }

        REQUIRE(InstanceCounter<Type>::get_counter() == 2 * initial_size);
        REQUIRE(InstanceCounter<Type>::get_counter_move() == 2 * initial_size);
        REQUIRE(smap.size() == 2 * initial_size);

        smap.clear();

        REQUIRE(InstanceCounter<Type>::get_counter() == 0);
        REQUIRE(InstanceCounter<Type>::get_counter_move() == 0);
        REQUIRE(smap.size() == 0);
        REQUIRE(smap.empty());
    }

    // Copy construction and assignment
    SECTION("copy")
    {
        REQUIRE(InstanceCounter<Type>::get_counter() == 0);

        for (size_t i = 0; i < initial_size; ++i) {
            smap.emplace();
        }

        REQUIRE(InstanceCounter<Type>::get_counter() == initial_size);

        decltype(smap) smap3{ initial_size };

        {
            // Copy construction
            decltype(smap) smap2{ smap };
            REQUIRE(InstanceCounter<Type>::get_counter() == 2 * initial_size);

            // Copy assignment
            smap3 = smap2;
            REQUIRE(InstanceCounter<Type>::get_counter() == 3 * initial_size);
        } // destruction of smap2

        REQUIRE(InstanceCounter<Type>::get_counter() == 2 * initial_size);
    }

    // Move construction and assignment
    SECTION("move")
    {
        REQUIRE(InstanceCounter<Type>::get_counter() == 0);

        Slot_map_handle handle;

        for (size_t i = 0; i < initial_size; ++i) {
            handle = smap.emplace();
        }

        REQUIRE(InstanceCounter<Type>::get_counter() == initial_size);

        // Move construction
        decltype(smap) smap2{ std::move(smap) };
        REQUIRE(InstanceCounter<Type>::get_counter() == initial_size);

        // Move assignment
        decltype(smap) smap3{ initial_size };
        smap3 = std::move(smap2);
        REQUIRE(InstanceCounter<Type>::get_counter() == initial_size);

        // Check moved from (should be valid state)
        REQUIRE(smap.empty());         // NOLINT
        REQUIRE(smap.capacity() == 0); // NOLINT
        smap.resize(initial_size);     // NOLINT
        smap.emplace();                // NOLINT
        REQUIRE(InstanceCounter<Type>::get_counter() == 26);
        REQUIRE(!smap.is_handle_valid(handle)); // NOLINT

        REQUIRE(smap2.empty());         // NOLINT
        REQUIRE(smap2.capacity() == 0); // NOLINT
        smap2.resize(initial_size);     // NOLINT
        smap2.emplace();                // NOLINT
        REQUIRE(InstanceCounter<Type>::get_counter() == 27);
    }

    // There was a bug where attempting to erase the last element led to the
    // Slot_map attempting to move the (destroyed) last element onto itself.
    // CountingType detects such situations and throws.
    SECTION("can_erase_last")
    {
        Slot_map_handle handle;

        for (size_t i = 0; i < initial_size; ++i) {
            handle = smap.emplace();
        }

        smap.erase(handle);
    }

    // Make sure STL algorithms and patterns work
    SECTION("algorithms")
    {
        for (size_t i = 0; i < initial_size; ++i) {
            smap.emplace(i);
        }

        std::array<Type*, initial_size> ptrs{};
        size_t i = 0;
        for (auto& elem : smap) {
            ptrs[i++] = &elem;
        }
        for (Type* p : ptrs) {
            REQUIRE(p != nullptr);
        }

        std::mt19937 g{ 75571296 };
        std::shuffle(smap.begin(), smap.end(), g);

        auto it = std::remove_if(smap.begin(), smap.end(), [](auto& elem) {
            return elem.get_value_checked() > 15;
        });

        smap.erase(it, smap.end());

        REQUIRE(smap.size() == 16);

        std::for_each(smap.begin(), smap.end(), [](auto& elem) {
            REQUIRE(elem.get_value_checked() <= 15);
        });

        std::sort(smap.begin(), smap.end(), [](auto& elemL, auto& elemR) {
            return elemL.get_value_checked() < elemR.get_value_checked();
        });

        size_t max = 0;

        for (auto& elem : smap) {
            REQUIRE(max <= elem.get_value_checked());
            max = elem.get_value_checked();
        }

        REQUIRE(max == 15);
    }
}

TEST_CASE("Large Slot_map")
{
    Slot_map<std::string> smap(10000);

    for (uint32_t i = 0; i < 5000; ++i) {
        smap.insert(" Insert string");
    }

    for (uint32_t i = 5000; i < 10000; ++i) {
        smap.emplace("Emplace string");
    }

    REQUIRE(*smap.begin() == " Insert string");
    REQUIRE(*(smap.begin() + 5000) == "Emplace string");
}

// Check that objects are properly destroyed on Slot_map destruction
TEST_CASE("Slot_map: check element destruction")
{
    constexpr size_t smap_size = 25;
    REQUIRE(InstanceCounter<Type>::get_counter_move() == 0);
    REQUIRE(InstanceCounter<Type>::get_counter() == 0);

    {
        Slot_map<Type> smap{ smap_size };
        for (size_t i = 0; i < smap_size; ++i) {
            smap.emplace();
        }

        REQUIRE(InstanceCounter<Type>::get_counter() == smap_size);
    }

    REQUIRE(InstanceCounter<Type>::get_counter() == 0);
}

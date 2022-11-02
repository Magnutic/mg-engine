#include "catch.hpp"

#include <mg/utils/mg_enum_map.h>

TEST_CASE("EnumMap basic test")
{
    MG_DEFINE_ENUM(MyEnum, A, B, C, D);
    Mg::EnumMap<MyEnum, int> map;

    // Nothing to iterate over.
    for ([[maybe_unused]] const auto& v : map) {
        CHECK(false);
    }

    // Simple assignment
    map[MyEnum::A] = 1;
    CHECK(map[MyEnum::A] == 1);

    {
        int i = 0;
        for (const auto& [key, value] : map) {
            CHECK(value == 1);
            ++i;
        }
        CHECK(i == 1);
    }

    // Implicit construction
    CHECK(map[MyEnum::B] == 0);

    map[MyEnum::B] = 2;

    // Get unset value gives nullptr.
    CHECK(map.get(MyEnum::C) == nullptr);

    map[MyEnum::C] = 3;

    // Get value that was set gives valid pointer.
    CHECK(*map.get(MyEnum::C) == 3);

    // Set unset value
    map.set(MyEnum::D, 4);
    CHECK(*map.get(MyEnum::D) == 4);

    // Set value that was already set
    map.set(MyEnum::D, -(*map.get(MyEnum::D)));
    CHECK(map[MyEnum::D] == -4);

    map[MyEnum::D] = 4;

    // Iterate and check values
    {
        int i = 0;
        for (const auto& [key, value] : map) {
            CHECK(++i == value);
        }
        CHECK(i == 4);
    }

    CHECK(map[MyEnum::A] == 1);
    CHECK(map[MyEnum::B] == 2);
    CHECK(map[MyEnum::C] == 3);
    CHECK(map[MyEnum::D] == 4);

    map[MyEnum::B] = -map[MyEnum::B];

    CHECK(map[MyEnum::A] == 1);
    CHECK(map[MyEnum::B] == -2);
    CHECK(map[MyEnum::C] == 3);
    CHECK(map[MyEnum::D] == 4);
}

TEST_CASE("EnumMap non-default-constructible value")
{
    struct IntWrapper {
        int i;

        IntWrapper(int _i) : i(_i) {}
        operator int() const { return i; }
    };

    MG_DEFINE_ENUM(MyEnum, A, B, C, D);
    Mg::EnumMap<MyEnum, IntWrapper> map;

    // Nothing to iterate over.
    for ([[maybe_unused]] const auto& v : map) {
        CHECK(false);
    }

    // Simple assignment
    map.set(MyEnum::A, 1);
    CHECK(*map.get(MyEnum::A) == 1);

    {
        int i = 0;
        for (const auto& [key, value] : map) {
            CHECK(value == 1);
            ++i;
        }
        CHECK(i == 1);
    }

    map.set(MyEnum::B, 2);

    // Get unset value gives nullptr.
    CHECK(map.get(MyEnum::C) == nullptr);

    map.set(MyEnum::C, 3);

    // Get value that was set gives valid pointer.
    CHECK(*map.get(MyEnum::C) == 3);

    // Set unset value
    map.set(MyEnum::D, 4);
    CHECK(*map.get(MyEnum::D) == 4);

    // Set value that was already set
    map.set(MyEnum::D, -(*map.get(MyEnum::D)));
    CHECK(*map.get(MyEnum::D) == -4);

    int four = map.set(MyEnum::D, 4);
    CHECK(four == 4);

    // Iterate and check values
    {
        int i = 0;
        for (const auto& [key, value] : map) {
            CHECK(++i == value);
        }
        CHECK(i == 4);
    }

    CHECK(*map.get(MyEnum::A) == 1);
    CHECK(*map.get(MyEnum::B) == 2);
    CHECK(*map.get(MyEnum::C) == 3);
    CHECK(*map.get(MyEnum::D) == 4);

    map.set(MyEnum::B, -(*map.get(MyEnum::B)));

    CHECK(*map.get(MyEnum::A) == 1);
    CHECK(*map.get(MyEnum::B) == -2);
    CHECK(*map.get(MyEnum::C) == 3);
    CHECK(*map.get(MyEnum::D) == 4);
}

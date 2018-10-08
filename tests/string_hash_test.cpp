#include "catch.hpp"

#include <cstring>
#include <string>
#include <string_view>

#include <mg/core/mg_identifier.h>

static uint32_t testFNV1a(const char* str)
{
    const size_t length = strlen(str);
    uint32_t     hash   = 2166136261u;

    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint32_t>(*str++);
        hash *= 16777619u;
    }

    return hash;
}

TEST_CASE("String hash test")
{
    // The following lines are intended for manually investigating the
    // generated assembly, i.e. that hashes were calculated at compile-time.
    // How to automate that?
    printf("%u\n", Mg::Identifier{ "Hello I'm a string" }.hash());
    printf("%u\n", Mg::Identifier{ "This is a string" }.hash());

    std::string str{ "a string" };
    printf("%u\n", Mg::Identifier::from_runtime_string(str).hash());

    auto pre_hashed = Mg::Identifier("a string").hash();
    auto preHashed2 = Mg::Identifier("This is a string");

    CHECK(pre_hashed == testFNV1a("a string"));
    CHECK(preHashed2.hash() == testFNV1a("This is a string"));

    REQUIRE(testFNV1a("ALongStringWithManyCharacters") ==
            Mg::Identifier{ "ALongStringWithManyCharacters" }.hash());

    CHECK(Mg::Identifier{ "a" }.hash() != Mg::Identifier{ "b" }.hash());

    Mg::Identifier id0{ "id0" };
    Mg::Identifier id1{ "id1" };
    Mg::Identifier id2{ "id2" };

    id0 = id1;
    REQUIRE(id0.str_view() == "id1");
    REQUIRE(id0.str_view() == "id1");

    id1 = Mg::Identifier::from_runtime_string(id2.c_str());
    REQUIRE(id1.str_view() == "id2");
    REQUIRE(id1.str_view() == "id2");

    id2 = "id0";
    REQUIRE(id2.str_view() == "id0");
    REQUIRE(id2.str_view() == "id0");
}

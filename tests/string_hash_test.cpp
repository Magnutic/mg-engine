#include "catch.hpp"

#include <cstring>
#include <string>
#include <string_view>

#define MG_CONTRACT_VIOLATION_THROWS 1 // Allow MG_ASSERT-failures to throw exceptions.
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

    std::string    long_string = "a string that is long enough to not be subject to SSO";
    Mg::Identifier dynamic_long_identifier = Mg::Identifier::from_runtime_string(long_string);

    REQUIRE(dynamic_long_identifier ==
            Mg::Identifier{ "a string that is long enough to not be subject to SSO" });
}

TEST_CASE("Hash collisions")
{
    // Identifier should work correctly even in the presence of hash collisions.
    // Hash collisions for FNV-1a found here:
    // https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed#145633

    Mg::Identifier altarage = "altarage";
    Mg::Identifier zinke    = "zinke";
    REQUIRE(altarage != zinke);

    Mg::Identifier costarring = "costarring";
    Mg::Identifier liquid     = "liquid";
    REQUIRE(costarring != liquid);

    Mg::Identifier declinate = "declinate";
    Mg::Identifier macallums = "macallums";
    REQUIRE(declinate != macallums);

    Mg::Identifier other_altarage = "altarage";
    auto           runtime_zinke  = Mg::Identifier::from_runtime_string(std::string("zin") + "ke");
    REQUIRE(other_altarage != zinke);
    REQUIRE(altarage != runtime_zinke);

    // It should be possible to create colliding runtime Identifiers;
    auto runtime_altarage = Mg::Identifier::from_runtime_string(std::string("altar") + "age");

    REQUIRE(runtime_altarage.str_view() == altarage.str_view());
    REQUIRE(runtime_zinke.str_view() == zinke.str_view());

    REQUIRE(runtime_altarage == altarage);
    REQUIRE(runtime_altarage != runtime_zinke);
    REQUIRE(runtime_zinke == zinke);
}

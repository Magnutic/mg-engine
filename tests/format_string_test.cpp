#include "catch.hpp"

#include <mg/utils/mg_format_string.h>

int         i                = 1;
float       f                = 1.0f;
double      d                = 1.5;
const char* c_str            = "c_str";
const char  string_literal[] = "string_literal";

TEST_CASE("BasicFormatting")
{
    REQUIRE(Mg::format_string("passthrough") == "passthrough");

    REQUIRE(Mg::format_string("int i: %d", i) == "int i: 1");
    REQUIRE(Mg::format_string("float f: %f", f) == "float f: 1.000000");
    REQUIRE(Mg::format_string("double d: %f", d) == "double d: 1.500000");

    REQUIRE(Mg::format_string("const char* c_str: %s", c_str) == "const char* c_str: c_str");

    REQUIRE(Mg::format_string("const char string_literal[]: %s", string_literal) ==
            "const char string_literal[]: string_literal");
}

// Unlike printf, Mg::format_string does not rely on type specifiers.
TEST_CASE("TypeSpecifierIndependence")
{
    REQUIRE(Mg::format_string("passthrough") == "passthrough");

    REQUIRE(Mg::format_string("int i: %s", i) == "int i: 1");
    REQUIRE(Mg::format_string("float f: %lld", f) == "float f: 1.000000");
    REQUIRE(Mg::format_string("double d: %u", d) == "double d: 1.500000");

    REQUIRE(Mg::format_string("const char* c_str: %llu", c_str) == "const char* c_str: c_str");

    REQUIRE(Mg::format_string("const char string_literal[]: %5u", string_literal) ==
            "const char string_literal[]: string_literal");
}

TEST_CASE("WidthAndPrecisionSpecifiers")
{
    REQUIRE(Mg::format_string("%11.2f", f) == "       1.00");
    REQUIRE(Mg::format_string("%011.0s", f) == "00000000001");
    REQUIRE(Mg::format_string("%#011.0f", f) == "0000000001.");
    REQUIRE(Mg::format_string("%#11.4f", d) == "     1.5000");
    REQUIRE(Mg::format_string("%#-11.4f", d) == "1.5000     ");
}

TEST_CASE("HexSpecifier")
{
    REQUIRE(Mg::format_string("%x", 1) == "1");
    REQUIRE(Mg::format_string("%#x", 26) == "0x1a");
    REQUIRE(Mg::format_string("%#X", 26) == "0X1A");
}

// TODO: hexfloat, scientific, oct

// Test multiple specifiers in one call to check for formatting leaks
TEST_CASE("FormattingDoesNotLeak")
{
    REQUIRE(Mg::format_string("%05.2f, %f", 1.0f, 1.0f) == "01.00, 1.000000");
    REQUIRE(Mg::format_string("%#x, %d", 26, 26) == "0x1a, 26");
    REQUIRE(Mg::format_string("%06d, %6d", 1, 1) == "000001,      1");
    REQUIRE(Mg::format_string("%+d, %d", 1, 1) == "+1, 1");
}

// Make sure '%%' is always printed as a single percent sign, even if there
// are no other specifiers.
TEST_CASE("PercentSign")
{
    REQUIRE(Mg::format_string("aaa %% bbb") == "aaa % bbb");
    REQUIRE(Mg::format_string("%i %%", i) == "1 %");
}


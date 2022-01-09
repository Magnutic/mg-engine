#include "catch.hpp"

#include <cmath>

#include <mg/utils/mg_math_utils.h>
#include <mg/utils/mg_point_normal_plane.h>
#include <mg/utils/mg_string_utils.h>

using namespace Mg;

TEST_CASE("tokenize_string")
{
    auto tokens = tokenize_string(" \t this is \ta string   ", " \t");
    std::string string = "hello";

    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0] == "this");
    REQUIRE(tokens[1] == "is");
    REQUIRE(tokens[2] == "a");
    REQUIRE(tokens[3] == "string");

    tokens = tokenize_string("another :string:here:", ":");

    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0] == "another ");
    REQUIRE(tokens[1] == "string");
    REQUIRE(tokens[2] == "here");
}

TEST_CASE("split_string")
{
    auto parts = split_string_on_char("A string = that I = am splitting", '=');
    REQUIRE(parts.first == "A string ");
    REQUIRE(parts.second == " that I = am splitting");

    parts = split_string_on_char("A string without the split char", '=');
    REQUIRE(parts.first == "A string without the split char");
    REQUIRE(parts.second == "");
}

TEST_CASE("trim")
{
    auto str = trim(" \t\n A string \t \n to trim  \t \n");
    REQUIRE(str == "A string \t \n to trim");

    str = trim("trimmed string");
    REQUIRE(str == "trimmed string");
}

TEST_CASE("find_any_of")
{
    REQUIRE(find_any_of("asdfjaek\tsss \nderp", "\t \n") == 8);
    REQUIRE(find_any_of("StringWithNoWhitespace", "\t \n") == std::string::npos);
}

TEST_CASE("to_lower and to_upper")
{
    REQUIRE(to_lower("A MiXeD cAsE sTrInG") == "a mixed case string");
    REQUIRE(to_upper("A MiXeD cAsE sTrInG") == "A MIXED CASE STRING");
}

TEST_CASE("sign")
{
    REQUIRE(sign(0.0f) == 0);
    REQUIRE(sign(-1.0f) == -1);
    REQUIRE(sign(1.0f) == 1);

    REQUIRE(sign(0.00001f) == 1);
    REQUIRE(sign(-0.0000000000001) == -1);

    // The following checks fail when compiled with -ffast-math on GCC-6.2.0
    // Is this a problem? TODO: investigate
    /*
     REQUIRE(sign(std::nextafter(0.0, 1.0)) == 1);
     REQUIRE(sign(std::nextafter(0.0, -1.0)) == -1);
     */
}

TEST_CASE("round_to_int")
{
    REQUIRE(round<int>(0.499f) == 0);
    REQUIRE(round<int>(0.0f) == 0);
    REQUIRE(round<int>(0.501f) == 1);
    REQUIRE(round<int>(0.5f) == 1);
    REQUIRE(round<int>(-0.499f) == 0);
    REQUIRE(round<int>(-0.5f) == -1);
    REQUIRE(round<int>(-0.501f) == -1);
}

TEST_CASE("string_to")
{
    REQUIRE(string_to<float>("1").second == 1.0);
    REQUIRE(string_to<float>("-1").second == -1.0);
    REQUIRE(string_to<float>("1e5").second == 1e5f);
}

TEST_CASE("string_from")
{
    REQUIRE(string_from(1.0f) == "1");
    REQUIRE(string_from(1.05f) == "1.05");
    REQUIRE(string_from(-1.0f) == "-1");
    REQUIRE(string_from(2e5f) == "200000");
    REQUIRE(string_from(1e10f) == "1e+10");
}

TEST_CASE("is_prefix_of")
{
    REQUIRE(is_prefix_of("_", "_"));
    REQUIRE(is_prefix_of("_", "_abc"));
    REQUIRE(is_prefix_of("", "_abc"));
    REQUIRE(is_prefix_of("", ""));
    REQUIRE(is_prefix_of("asd", "asd"));
    REQUIRE(is_prefix_of("asd", "asdf"));
    REQUIRE(is_prefix_of("", "asd"));

    REQUIRE(!is_prefix_of("asdf", "fasd"));
    REQUIRE(!is_prefix_of("asd", "fasd"));
    REQUIRE(!is_prefix_of("asdasd", "asd"));
}

TEST_CASE("is_suffix_of")
{
    REQUIRE(is_suffix_of("_", "_"));
    REQUIRE(is_suffix_of("_", "abc_"));
    REQUIRE(is_suffix_of("", "abc_"));
    REQUIRE(is_suffix_of("", ""));
    REQUIRE(is_suffix_of("asd", "asd"));
    REQUIRE(is_suffix_of("asd", "fasd"));
    REQUIRE(is_suffix_of("", "fasd"));

    REQUIRE(!is_suffix_of("asdf", "fasd"));
    REQUIRE(!is_suffix_of("fasd", "asd"));
    REQUIRE(!is_suffix_of("asdasd", "asd"));
}

TEST_CASE("PointNormalPlane")
{
    auto p{ PointNormalPlane::from_point_and_normal({ 1.0f, 2.0f, 3.0f }, { -1.0f, 1.0f, 0.0f }) };
    glm::vec3 point{ 5.0f, 5.0f, 5.0f };

    auto sgn_dist = signed_distance_to_plane(p, point);
    auto dist = distance_to_plane(p, point);

    REQUIRE(sgn_dist == Approx(-1.0f / std::sqrt(2.0f)));
    REQUIRE(dist == Approx(1.0f / std::sqrt(2.0f)));
}

TEST_CASE("min/max/clamp int")
{
    REQUIRE(min(0, 1) == 0);
    REQUIRE(min(1, 0) == 0);
    REQUIRE(min(0, -1) == -1);

    REQUIRE(max(0, 1) == 1);
    REQUIRE(max(1, 0) == 1);
    REQUIRE(max(0, 0) == 0);

    REQUIRE(clamp(0, -1, 1) == 0);
    REQUIRE(clamp(-1, -1, 1) == -1);
    REQUIRE(clamp(-2, -1, 1) == -1);
    REQUIRE(clamp(1, -1, 1) == 1);
    REQUIRE(clamp(2, -1, 1) == 1);
}

TEST_CASE("min/max/clamp float")
{
    REQUIRE(min(0.0f, 1.0f) == 0.0f);
    REQUIRE(min(1.0f, -1.0f) == -1.0f);
    REQUIRE(min(0.0f, 0.0f) == 0.0f);

    REQUIRE(max(0.0f, 1.0f) == 1.0f);
    REQUIRE(max(1.0f, 0.0f) == 1.0f);
    REQUIRE(max(-1.0f, 0.0f) == 0.0f);

    REQUIRE(clamp(0.0f, -1.0f, 1.0f) == 0.0f);
    REQUIRE(clamp(-1.0f, -1.0f, 1.0f) == -1.0f);
    REQUIRE(clamp(-2.0f, -1.0f, 1.0f) == -1.0f);
    REQUIRE(clamp(1.0f, -1.0f, 1.0f) == 1.0f);
    REQUIRE(clamp(2.0f, -1.0f, 1.0f) == 1.0f);
}

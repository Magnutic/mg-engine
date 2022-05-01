#include "catch.hpp"

#include <cmath>

#include <mg/utils/mg_iteration_utils.h>
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
    REQUIRE(to_lower("R\xc3\x84KSM\xc3\x96RG\xc3\x85S") == "r\xc3\xa4ksm\xc3\xb6rg\xc3\xa5s");
    REQUIRE(to_upper("R\xc3\xa4ksm\xc3\xb6rg\xc3\xa5s") == "R\xc3\x84KSM\xc3\x96RG\xc3\x85S");

    const std::string frenchPangramUpper =
        "\x56\x4f\x49\x58\x20\x41\x4d\x42\x49\x47\x55\xc3\x8b\x20\x44\xe2\x80\x99\x55"
        "\x4e\x20\x43\xc5\x92\x55\x52\x20\x51\x55\x49\x20\x41\x55\x20\x5a\xc3\x89\x50"
        "\x48\x59\x52\x20\x50\x52\xc3\x89\x46\xc3\x88\x52\x45\x20\x4c\x45\x53\x20\x4a"
        "\x41\x54\x54\x45\x53\x20\x44\x45\x20\x4b\x49\x57\x49\x53";

    const std::string frenchPangramLower =
        "\x76\x6f\x69\x78\x20\x61\x6d\x62\x69\x67\x75\xc3\xab\x20\x64\xe2\x80\x99\x75\x6e\x20"
        "\x63\xc5\x93\x75\x72\x20\x71\x75\x69\x20\x61\x75\x20\x7a\xc3\xa9\x70\x68\x79\x72\x20"
        "\x70\x72\xc3\xa9\x66\xc3\xa8\x72\x65\x20\x6c\x65\x73\x20\x6a\x61\x74\x74\x65\x73\x20"
        "\x64\x65\x20\x6b\x69\x77\x69\x73";

    REQUIRE(to_lower(frenchPangramUpper) == frenchPangramLower);
    REQUIRE(to_lower(frenchPangramLower) == frenchPangramLower);
    REQUIRE(to_upper(frenchPangramLower) == frenchPangramUpper);
    REQUIRE(to_upper(frenchPangramUpper) == frenchPangramUpper);
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

TEST_CASE("substring_until")
{
    REQUIRE(substring_until("abcdefabcdef", 'd') == "abc");
    REQUIRE(substring_until("abcdefabcdef", 'a') == "");
    REQUIRE(substring_until("abcdefabcdef", 'f') == "abcde");
    REQUIRE(substring_until("abcdefabcdef", 'x') == "");
    REQUIRE(substring_until("", 'd') == "");

    REQUIRE(substring_until_last("abcdefabcdef", 'd') == "abcdefabc");
    REQUIRE(substring_until_last("abcdefabcdef", 'a') == "abcdef");
    REQUIRE(substring_until_last("abcdefabcdef", 'f') == "abcdefabcde");
    REQUIRE(substring_until_last("abcdefabcdef", 'x') == "");
    REQUIRE(substring_until_last("", 'd') == "");
}

TEST_CASE("substring_after")
{
    REQUIRE(substring_after("abcdefabcdef", 'd') == "efabcdef");
    REQUIRE(substring_after("abcdefabcdef", 'a') == "bcdefabcdef");
    REQUIRE(substring_after("abcdefabcdef", 'f') == "abcdef");
    REQUIRE(substring_after("abcdefabcdef", 'x') == "");
    REQUIRE(substring_after("", 'd') == "");

    REQUIRE(substring_after_last("abcdefabcdef", 'd') == "ef");
    REQUIRE(substring_after_last("abcdefabcdef", 'a') == "bcdef");
    REQUIRE(substring_after_last("abcdefabcdef", 'f') == "");
    REQUIRE(substring_after_last("abcdefabcdef", 'x') == "");
    REQUIRE(substring_after_last("", 'd') == "");
}

TEST_CASE("replace_char")
{
    REQUIRE(replace_char("abcdef", 'x', 'd') == "abcdef");
    REQUIRE(replace_char("abcdef", 'c', 'd') == "abddef");
    REQUIRE(replace_char("abcdefabcdef", 'c', 'd') == "abddefabddef");
    REQUIRE(replace_char("c", 'c', 'd') == "d");
    REQUIRE(replace_char("f", 'c', 'd') == "f");
    REQUIRE(replace_char("", 'c', 'd') == "");
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

template<typename T> void assert_ref_is_const(T&)
{
    static_assert(std::is_const_v<T>);
}

template<typename T> void assert_ref_is_not_const(T&)
{
    static_assert(!std::is_const_v<T>);
}

TEST_CASE("iterate_adjacent empty")
{
    std::vector<int> empty = {};
    size_t num_iterations = 0;
    for ([[maybe_unused]] auto&& [a, b] : iterate_adjacent(empty)) {
        ++num_iterations;
    }

    REQUIRE(num_iterations == 0);
}

TEST_CASE("iterate_adjacent one element")
{
    std::vector<int> one_element = { 1 };
    size_t num_iterations = 0;
    for ([[maybe_unused]] auto&& [a, b] : iterate_adjacent(one_element)) {
        ++num_iterations;
    }

    REQUIRE(num_iterations == 0);
}

TEST_CASE("iterate_adjacent two element")
{
    std::vector<int> two_elements = { 1, 2 };
    size_t num_iterations = 0;
    for (auto&& [a, b] : iterate_adjacent(two_elements)) {
        ++num_iterations;
        REQUIRE(a == 1);
        REQUIRE(b == 2);
    }

    REQUIRE(num_iterations == 1);
}

TEST_CASE("iterate_adjacent many elements")
{
    std::vector<int> values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    size_t num_iterations = 0;
    for (auto&& [a, b] : iterate_adjacent(values)) {
        ++num_iterations;
        REQUIRE(b == a + 1);
    }

    REQUIRE(num_iterations == values.size() - 1);
}

TEST_CASE("iterate_adjacent mutable")
{
    std::vector<int> values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    size_t num_iterations = 0;
    for (auto&& [a, b] : iterate_adjacent(values)) {
        ++num_iterations;
        b += a;
    }

    REQUIRE(num_iterations == values.size() - 1);
    REQUIRE(values == std::vector{ 1, 3, 6, 10, 15, 21, 28, 36, 45 });
}

TEST_CASE("iterate_adjacent const")
{
    const std::vector<int> const_values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::vector<int> values = const_values;

    {
        size_t num_iterations = 0;
        for (auto&& [a, b] : iterate_adjacent(std::as_const(values))) {
            assert_ref_is_const(a);
            assert_ref_is_const(b);
            ++num_iterations;
        }
        REQUIRE(num_iterations == values.size() - 1);
    }

    {
        size_t num_iterations = 0;
        for ([[maybe_unused]] auto&& [a, b] : iterate_adjacent(const_values)) {
            assert_ref_is_const(a);
            assert_ref_is_const(b);
            ++num_iterations;
        }
        REQUIRE(num_iterations == const_values.size() - 1);
    }
}

TEST_CASE("enumerate empty")
{
    std::vector<int> empty = {};
    size_t num_iterations = 0;
    for ([[maybe_unused]] auto&& [i, v] : enumerate(empty, size_t(0))) {
        ++num_iterations;
    }

    REQUIRE(num_iterations == 0);
}

TEST_CASE("enumerate one element")
{
    std::vector<int> one_element = { 1 };
    size_t num_iterations = 0;
    for (auto&& [i, v] : enumerate(one_element, size_t(0))) {
        REQUIRE(num_iterations == i);
        ++num_iterations;
    }

    REQUIRE(num_iterations == 1);
}

TEST_CASE("enumerate")
{
    std::vector<int> values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    {
        size_t num_iterations = 0;
        for (auto&& [i, v] : enumerate(values, size_t(0))) {
            REQUIRE(i == num_iterations);
            ++num_iterations;
        }

        REQUIRE(num_iterations == values.size());
    }

    {
        size_t num_iterations = 0;
        for (auto&& [i, v] : enumerate(values, size_t(5))) {
            REQUIRE(i == num_iterations + 5);
            ++num_iterations;
        }

        REQUIRE(num_iterations == values.size());
    }

    {
        size_t num_iterations = 0;
        for (auto&& [i, v] : enumerate(values, int(0))) {
            static_assert(std::is_same_v<std::decay_t<decltype(i)>, int>);
            REQUIRE(static_cast<size_t>(i) == num_iterations);
            ++num_iterations;
        }

        REQUIRE(num_iterations == values.size());
    }

    {
        size_t num_iterations = 0;
        for (auto&& [i, v] : enumerate(values, -5)) {
            static_assert(std::is_same_v<std::decay_t<decltype(i)>, int>);
            REQUIRE(static_cast<size_t>(i + 5) == num_iterations);
            ++num_iterations;
        }

        REQUIRE(num_iterations == values.size());
    }
}

TEST_CASE("enumerate mutable")
{
    std::vector<int> values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    size_t num_iterations = 0;
    for (auto&& [i, v] : enumerate(values, 0)) {
        v += 1;
        ++num_iterations;
    }

    REQUIRE(num_iterations == values.size());
    REQUIRE(values == std::vector{ 2, 3, 4, 5, 6, 7, 8, 9, 10 });
}

TEST_CASE("enumerate const")
{
    const std::vector<int> const_values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::vector<int> values = const_values;

    {
        size_t num_iterations = 0;
        for (auto&& [i, v] : enumerate(std::as_const(values), 0)) {
            assert_ref_is_const(v);
            REQUIRE(static_cast<size_t>(i) == num_iterations);
            ++num_iterations;
        }
        REQUIRE(num_iterations == values.size());
    }

    {
        size_t num_iterations = 0;
        for (auto&& [i, v] : enumerate(const_values, 0)) {
            assert_ref_is_const(v);
            REQUIRE(static_cast<size_t>(i) == num_iterations);
            ++num_iterations;
        }
        REQUIRE(num_iterations == const_values.size());
    }
}

TEST_CASE("zip empty")
{
    std::vector<int> i_empty = {};
    std::vector<int> i_non_empty = { 1 };
    std::vector<std::string> s_empty = {};
    std::vector<std::string> s_non_empty = { "test" };

    size_t num_iterations = 0;

    for ([[maybe_unused]] auto&& [i, s] : zip(i_empty, s_empty)) {
        ++num_iterations;
    }
    for ([[maybe_unused]] auto&& [i, s] : zip(i_empty, s_non_empty)) {
        ++num_iterations;
    }
    for ([[maybe_unused]] auto&& [i, s] : zip(i_non_empty, s_empty)) {
        ++num_iterations;
    }

    REQUIRE(num_iterations == 0);
}

TEST_CASE("zip one element")
{
    std::vector<int> i_one_element = { 1 };
    std::vector<std::string> s_one_element = { "test" };
    size_t num_iterations = 0;

    for (auto&& [i, s] : zip(i_one_element, s_one_element)) {
        REQUIRE(i == 1);
        REQUIRE(s == "test");
        ++num_iterations;
    }

    REQUIRE(num_iterations == 1);
}

TEST_CASE("zip one element const")
{
    std::vector<int> i_one_element = { 1 };
    std::vector<std::string> s_one_element = { "test" };
    const std::vector<int> const_i_one_element = { 1 };
    const std::vector<std::string> const_s_one_element = { "test" };

    size_t num_iterations = 0;

    for (auto&& [i, s] : zip(const_i_one_element, const_s_one_element)) {
        assert_ref_is_const(i);
        assert_ref_is_const(s);
        REQUIRE(i == 1);
        REQUIRE(s == "test");
        ++num_iterations;
    }

    for (auto&& [i, s] : zip(std::as_const(i_one_element), std::as_const(s_one_element))) {
        assert_ref_is_const(i);
        assert_ref_is_const(s);
        REQUIRE(i == 1);
        REQUIRE(s == "test");
        ++num_iterations;
    }

    for (auto&& [i, s] : zip(std::as_const(i_one_element), std::as_const(s_one_element))) {
        assert_ref_is_const(i);
        assert_ref_is_const(s);
        REQUIRE(i == 1);
        REQUIRE(s == "test");
        ++num_iterations;
    }

    for (auto&& [i, s] : zip(const_i_one_element, s_one_element)) {
        assert_ref_is_const(i);
        assert_ref_is_not_const(s);
        REQUIRE(i == 1);
        REQUIRE(s == "test");
        ++num_iterations;
    }

    for (auto&& [i, s] : zip(i_one_element, const_s_one_element)) {
        assert_ref_is_not_const(i);
        assert_ref_is_const(s);
        REQUIRE(i == 1);
        REQUIRE(s == "test");
        ++num_iterations;
    }

    REQUIRE(num_iterations == 5);
}


TEST_CASE("zip two element")
{
    std::vector<int> i_two_elements = { 1, 2 };
    std::vector<std::string> s_two_elements = { "test_1", "test_2" };
    size_t num_iterations = 0;
    for (auto&& [i, s] : zip(i_two_elements, s_two_elements)) {
        REQUIRE(static_cast<size_t>(i) == num_iterations + 1);
        REQUIRE(s == (num_iterations == 0 ? "test_1" : "test_2"));
        ++num_iterations;
    }

    REQUIRE(num_iterations == 2);
}

TEST_CASE("zip different length 1")
{
    std::vector<int> i_values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::vector<std::string> s_values = { "A", "B", "C", "D" };
    size_t num_iterations = 0;
    for (auto&& [i, s] : zip(i_values, s_values)) {
        REQUIRE(static_cast<size_t>(i) == num_iterations + 1);
        REQUIRE(s == s_values[num_iterations]);
        ++num_iterations;
    }

    REQUIRE(num_iterations == 4);
}

TEST_CASE("zip different length 2")
{
    std::vector<int> i_values = { 1, 2, 3, 4, 5 };
    std::vector<std::string> s_values = { "A", "B", "C", "D", "E", "F", "G", "H" };
    size_t num_iterations = 0;
    for (auto&& [i, s] : zip(i_values, s_values)) {
        REQUIRE(static_cast<size_t>(i) == num_iterations + 1);
        REQUIRE(s == s_values[num_iterations]);
        ++num_iterations;
    }

    REQUIRE(num_iterations == 5);
}

#if TEST_COMPILE_ERROR_ON_ITERATION_UTILS_FROM_RVALUE_CONTAINER
TEST_CASE("Iteration utils cannot construct from rvalue")
{
    for ([[maybe_unused]] auto&& [i, s] : iterate_adjacent(std::vector{ 1, 2, 3 })) {
    }
    for ([[maybe_unused]] auto&& [i, s] : enumerate<int>(std::vector{ 1, 2, 3 })) {
    }
    for ([[maybe_unused]] auto&& [i, s] : zip(std::vector{ 1, 2, 3 }, std::vector{ 1, 2, 3 })) {
    }
}
#endif

#include "catch.hpp"

#include <mg/mg_unicode.h>
#include <mg/utils/mg_stl_helpers.h>

#include <string>

TEST_CASE("Unicode")
{
    using namespace Mg;

    std::string utf8_ascii = "hej";
    std::string utf8_latin1 = "\xC3\xA5\xC3\xA4\xC3\xB6";
    std::string utf8_hangul = "\xEC\x95\x88\xEB\x85\x95";

    std::u32string utf32_ascii = utf8_to_utf32(utf8_ascii);
    std::u32string utf32_latin1 = utf8_to_utf32(utf8_latin1);
    std::u32string utf32_hangul = utf8_to_utf32(utf8_hangul);

    SECTION("utf8_to_utf32")
    {
        CHECK(utf32_ascii == U"hej");
        CHECK(utf32_latin1 == U"\xE5\xE4\xF6");
        CHECK(utf32_hangul == U"\xC548\xB155");
    }

    SECTION("contains_codepoint")
    {
        for (const auto c : utf32_ascii) {
            CHECK(contains_codepoint(UnicodeBlock::Basic_Latin, c));
            CHECK(!contains_codepoint(UnicodeBlock::Balinese, c));
        }
        for (const auto c : utf32_latin1) {
            CHECK(contains_codepoint(UnicodeBlock::Latin_1_Supplement, c));
            CHECK(!contains_codepoint(UnicodeBlock::Aegean_Numbers, c));
        }
        for (const auto c : utf32_hangul) {
            CHECK(contains_codepoint(UnicodeBlock::Hangul_Syllables, c));
            CHECK(!contains_codepoint(UnicodeBlock::Basic_Latin, c));
        }
    }

    SECTION("unicode_block_containing")
    {
        for (const auto c : utf32_ascii) {
            CHECK(unicode_block_containing(c).value() == UnicodeBlock::Basic_Latin);
        }
        for (const auto c : utf32_latin1) {
            CHECK(unicode_block_containing(c).value() == UnicodeBlock::Latin_1_Supplement);
        }
        for (const auto c : utf32_hangul) {
            CHECK(unicode_block_containing(c).value() == UnicodeBlock::Hangul_Syllables);
        }
    }

    SECTION("unicode_ranges_for")
    {
        std::vector<UnicodeRange> unicode_ranges = unicode_ranges_for("abcdefåäö");
        CHECK(unicode_ranges.size() == 3);

        constexpr UnicodeRange a_to_f_range = { U'a', 6 };
        constexpr UnicodeRange ao_to_ae_range = { U'\xE4', 2 };
        constexpr UnicodeRange oe_range = { U'\xF6', 1 };

        CHECK(count(unicode_ranges, a_to_f_range) == 1);
        CHECK(count(unicode_ranges, ao_to_ae_range) == 1);
        CHECK(count(unicode_ranges, oe_range) == 1);
    }

    SECTION("merge_overlapping_ranges")
    {
        auto are_arrays_equal = [](const auto& lhs, const auto& rhs) {
            return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        };

        { // Non-overlapping case. Should not make any difference.
            std::vector<UnicodeRange> input({ get_unicode_range(UnicodeBlock::Basic_Latin),
                                              get_unicode_range(UnicodeBlock::Avestan) });

            auto merged_ranges = merge_overlapping_ranges(input);
            CHECK(are_arrays_equal(input, merged_ranges));
        }

        { // Adjacent case. Should merge.
            std::vector<UnicodeRange> input(
                { get_unicode_range(UnicodeBlock::Basic_Latin),
                  get_unicode_range(UnicodeBlock::Latin_1_Supplement) });

            std::vector<UnicodeRange> expected_result({ { 0, 256 } });

            auto merged_ranges = merge_overlapping_ranges(input);
            CHECK(are_arrays_equal(merged_ranges, expected_result));
        }

        { // Overlapping case 1
            std::vector<UnicodeRange> input({ { 128, 255 }, { 127, 1024 } });
            std::vector<UnicodeRange> expected_result({ { 127, 1024 } });
            auto merged_ranges = merge_overlapping_ranges(input);
            CHECK(are_arrays_equal(merged_ranges, expected_result));
        }

        { // Overlapping case 2
            std::vector<UnicodeRange> input({ { 128, 255 }, { 240, 20 } });
            std::vector<UnicodeRange> expected_result({ { 128, 255 } });
            auto merged_ranges = merge_overlapping_ranges(input);
            CHECK(are_arrays_equal(merged_ranges, expected_result));
        }

        { // Overlapping case 2
            std::vector<UnicodeRange> input({ { 50, 10 }, { 55, 10 }, { 128, 10 }, { 136, 5 } });
            std::vector<UnicodeRange> expected_result({ { 50, 15 }, { 128, 13 } });
            auto merged_ranges = merge_overlapping_ranges(input);
            CHECK(are_arrays_equal(merged_ranges, expected_result));
        }
    }
}

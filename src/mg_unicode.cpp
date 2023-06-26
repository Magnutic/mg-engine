//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/mg_unicode.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_iteration_utils.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

#include <string>

namespace Mg {

//--------------------------------------------------------------------------------------------------
// UnicodeRanges for the pre-defined UnicodeBlock values.
//--------------------------------------------------------------------------------------------------

constexpr UnicodeRange make_unicode_range(char32_t start, char32_t end)
{
    MG_ASSERT(start < end);
    return { start, as<uint32_t>(end - start) + 1 };
}

static constexpr std::array<UnicodeRange, 207> unicode_block_ranges = {
    { make_unicode_range(0x0, 0x7F),        make_unicode_range(0x80, 0xFF),
      make_unicode_range(0x100, 0x17F),     make_unicode_range(0x180, 0x24F),
      make_unicode_range(0x250, 0x2AF),     make_unicode_range(0x2B0, 0x2FF),
      make_unicode_range(0x300, 0x36F),     make_unicode_range(0x370, 0x3FF),
      make_unicode_range(0x400, 0x4FF),     make_unicode_range(0x500, 0x527),
      make_unicode_range(0x531, 0x58A),     make_unicode_range(0x591, 0x5F4),
      make_unicode_range(0x600, 0x6FF),     make_unicode_range(0x700, 0x74F),
      make_unicode_range(0x750, 0x77F),     make_unicode_range(0x780, 0x7B1),
      make_unicode_range(0x7C0, 0x7FA),     make_unicode_range(0x800, 0x83E),
      make_unicode_range(0x840, 0x85E),     make_unicode_range(0x900, 0x97F),
      make_unicode_range(0x981, 0x9FB),     make_unicode_range(0xA01, 0xA75),
      make_unicode_range(0xA81, 0xAF1),     make_unicode_range(0xB01, 0xB77),
      make_unicode_range(0xB82, 0xBFA),     make_unicode_range(0xC01, 0xC7F),
      make_unicode_range(0xC82, 0xCF2),     make_unicode_range(0xD02, 0xD7F),
      make_unicode_range(0xD82, 0xDF4),     make_unicode_range(0xE01, 0xE5B),
      make_unicode_range(0xE81, 0xEDD),     make_unicode_range(0xF00, 0xFDA),
      make_unicode_range(0x1000, 0x109F),   make_unicode_range(0x10A0, 0x10FC),
      make_unicode_range(0x1100, 0x11FF),   make_unicode_range(0x1200, 0x137C),
      make_unicode_range(0x1380, 0x1399),   make_unicode_range(0x13A0, 0x13F4),
      make_unicode_range(0x1400, 0x167F),   make_unicode_range(0x1680, 0x169C),
      make_unicode_range(0x16A0, 0x16F0),   make_unicode_range(0x1700, 0x1714),
      make_unicode_range(0x1720, 0x1736),   make_unicode_range(0x1740, 0x1753),
      make_unicode_range(0x1760, 0x1773),   make_unicode_range(0x1780, 0x17F9),
      make_unicode_range(0x1800, 0x18AA),   make_unicode_range(0x18B0, 0x18F5),
      make_unicode_range(0x1900, 0x194F),   make_unicode_range(0x1950, 0x1974),
      make_unicode_range(0x1980, 0x19DF),   make_unicode_range(0x19E0, 0x19FF),
      make_unicode_range(0x1A00, 0x1A1F),   make_unicode_range(0x1A20, 0x1AAD),
      make_unicode_range(0x1B00, 0x1B7C),   make_unicode_range(0x1B80, 0x1BB9),
      make_unicode_range(0x1BC0, 0x1BFF),   make_unicode_range(0x1C00, 0x1C4F),
      make_unicode_range(0x1C50, 0x1C7F),   make_unicode_range(0x1CD0, 0x1CF2),
      make_unicode_range(0x1D00, 0x1D7F),   make_unicode_range(0x1D80, 0x1DBF),
      make_unicode_range(0x1DC0, 0x1DFF),   make_unicode_range(0x1E00, 0x1EFF),
      make_unicode_range(0x1F00, 0x1FFE),   make_unicode_range(0x2000, 0x206F),
      make_unicode_range(0x2070, 0x209C),   make_unicode_range(0x20A0, 0x20B9),
      make_unicode_range(0x20D0, 0x20F0),   make_unicode_range(0x2100, 0x214F),
      make_unicode_range(0x2150, 0x2189),   make_unicode_range(0x2190, 0x21FF),
      make_unicode_range(0x2200, 0x22FF),   make_unicode_range(0x2300, 0x23F3),
      make_unicode_range(0x2400, 0x2426),   make_unicode_range(0x2440, 0x244A),
      make_unicode_range(0x2460, 0x24FF),   make_unicode_range(0x2500, 0x257F),
      make_unicode_range(0x2580, 0x259F),   make_unicode_range(0x25A0, 0x25FF),
      make_unicode_range(0x2600, 0x26FF),   make_unicode_range(0x2701, 0x27BF),
      make_unicode_range(0x27C0, 0x27EF),   make_unicode_range(0x27F0, 0x27FF),
      make_unicode_range(0x2800, 0x28FF),   make_unicode_range(0x2900, 0x297F),
      make_unicode_range(0x2980, 0x29FF),   make_unicode_range(0x2A00, 0x2AFF),
      make_unicode_range(0x2B00, 0x2B59),   make_unicode_range(0x2C00, 0x2C5E),
      make_unicode_range(0x2C60, 0x2C7F),   make_unicode_range(0x2C80, 0x2CFF),
      make_unicode_range(0x2D00, 0x2D25),   make_unicode_range(0x2D30, 0x2D7F),
      make_unicode_range(0x2D80, 0x2DDE),   make_unicode_range(0x2DE0, 0x2DFF),
      make_unicode_range(0x2E00, 0x2E31),   make_unicode_range(0x2E80, 0x2EF3),
      make_unicode_range(0x2F00, 0x2FD5),   make_unicode_range(0x2FF0, 0x2FFB),
      make_unicode_range(0x3000, 0x303F),   make_unicode_range(0x3041, 0x309F),
      make_unicode_range(0x30A0, 0x30FF),   make_unicode_range(0x3105, 0x312D),
      make_unicode_range(0x3131, 0x318E),   make_unicode_range(0x3190, 0x319F),
      make_unicode_range(0x31A0, 0x31BA),   make_unicode_range(0x31C0, 0x31E3),
      make_unicode_range(0x31F0, 0x31FF),   make_unicode_range(0x3200, 0x32FE),
      make_unicode_range(0x3300, 0x33FF),   make_unicode_range(0x3400, 0x4DB5),
      make_unicode_range(0x4DC0, 0x4DFF),   make_unicode_range(0x4E00, 0x9FCB),
      make_unicode_range(0xA000, 0xA48C),   make_unicode_range(0xA490, 0xA4C6),
      make_unicode_range(0xA4D0, 0xA4FF),   make_unicode_range(0xA500, 0xA62B),
      make_unicode_range(0xA640, 0xA697),   make_unicode_range(0xA6A0, 0xA6F7),
      make_unicode_range(0xA700, 0xA71F),   make_unicode_range(0xA720, 0xA7FF),
      make_unicode_range(0xA800, 0xA82B),   make_unicode_range(0xA830, 0xA839),
      make_unicode_range(0xA840, 0xA877),   make_unicode_range(0xA880, 0xA8D9),
      make_unicode_range(0xA8E0, 0xA8FB),   make_unicode_range(0xA900, 0xA92F),
      make_unicode_range(0xA930, 0xA95F),   make_unicode_range(0xA960, 0xA97C),
      make_unicode_range(0xA980, 0xA9DF),   make_unicode_range(0xAA00, 0xAA5F),
      make_unicode_range(0xAA60, 0xAA7B),   make_unicode_range(0xAA80, 0xAADF),
      make_unicode_range(0xAB01, 0xAB2E),   make_unicode_range(0xABC0, 0xABF9),
      make_unicode_range(0xAC00, 0xD7A3),   make_unicode_range(0xD7B0, 0xD7FB),
      make_unicode_range(0xD800, 0xDB7F),   make_unicode_range(0xDB80, 0xDBFF),
      make_unicode_range(0xDC00, 0xDFFF),   make_unicode_range(0xE000, 0xF8FF),
      make_unicode_range(0xF900, 0xFAD9),   make_unicode_range(0xFB00, 0xFB4F),
      make_unicode_range(0xFB50, 0xFDFD),   make_unicode_range(0xFE00, 0xFE0F),
      make_unicode_range(0xFE10, 0xFE19),   make_unicode_range(0xFE20, 0xFE26),
      make_unicode_range(0xFE30, 0xFE4F),   make_unicode_range(0xFE50, 0xFE6B),
      make_unicode_range(0xFE70, 0xFEFF),   make_unicode_range(0xFF01, 0xFFEE),
      make_unicode_range(0xFFF9, 0xFFFD),   make_unicode_range(0x10000, 0x1005D),
      make_unicode_range(0x10080, 0x100FA), make_unicode_range(0x10100, 0x1013F),
      make_unicode_range(0x10140, 0x1018A), make_unicode_range(0x10190, 0x1019B),
      make_unicode_range(0x101D0, 0x101FD), make_unicode_range(0x10280, 0x1029C),
      make_unicode_range(0x102A0, 0x102D0), make_unicode_range(0x10300, 0x10323),
      make_unicode_range(0x10330, 0x1034A), make_unicode_range(0x10380, 0x1039F),
      make_unicode_range(0x103A0, 0x103D5), make_unicode_range(0x10400, 0x1044F),
      make_unicode_range(0x10450, 0x1047F), make_unicode_range(0x10480, 0x104A9),
      make_unicode_range(0x10800, 0x1083F), make_unicode_range(0x10840, 0x1085F),
      make_unicode_range(0x10900, 0x1091F), make_unicode_range(0x10920, 0x1093F),
      make_unicode_range(0x10A00, 0x10A58), make_unicode_range(0x10A60, 0x10A7F),
      make_unicode_range(0x10B00, 0x10B3F), make_unicode_range(0x10B40, 0x10B5F),
      make_unicode_range(0x10B60, 0x10B7F), make_unicode_range(0x10C00, 0x10C48),
      make_unicode_range(0x10E60, 0x10E7E), make_unicode_range(0x11000, 0x1106F),
      make_unicode_range(0x11080, 0x110C1), make_unicode_range(0x12000, 0x1236E),
      make_unicode_range(0x12400, 0x12473), make_unicode_range(0x13000, 0x1342E),
      make_unicode_range(0x16800, 0x16A38), make_unicode_range(0x1B000, 0x1B001),
      make_unicode_range(0x1D000, 0x1D0F5), make_unicode_range(0x1D100, 0x1D1DD),
      make_unicode_range(0x1D200, 0x1D245), make_unicode_range(0x1D300, 0x1D356),
      make_unicode_range(0x1D360, 0x1D371), make_unicode_range(0x1D400, 0x1D7FF),
      make_unicode_range(0x1F000, 0x1F02B), make_unicode_range(0x1F030, 0x1F093),
      make_unicode_range(0x1F0A0, 0x1F0DF), make_unicode_range(0x1F100, 0x1F1FF),
      make_unicode_range(0x1F200, 0x1F251), make_unicode_range(0x1F300, 0x1F5FF),
      make_unicode_range(0x1F601, 0x1F64F), make_unicode_range(0x1F680, 0x1F6C5),
      make_unicode_range(0x1F700, 0x1F773), make_unicode_range(0x20000, 0x2A6D6),
      make_unicode_range(0x2A700, 0x2B734), make_unicode_range(0x2B740, 0x2B81D),
      make_unicode_range(0x2F800, 0x2FA1D), make_unicode_range(0xE0001, 0xE007F),
      make_unicode_range(0xE0100, 0xE01EF) }
};

UnicodeRange get_unicode_range(UnicodeBlock block)
{
    return unicode_block_ranges.at(static_cast<size_t>(block));
}

Opt<UnicodeBlock> unicode_block_containing(const char32_t codepoint)
{
    auto cmp = [](const UnicodeRange& r, const char32_t c) { return r.start + r.length < c; };
    auto it = lower_bound(unicode_block_ranges, codepoint, cmp);

    if (it == unicode_block_ranges.end() || !contains_codepoint(*it, codepoint)) {
        return nullopt;
    }
    const auto offset = std::distance(unicode_block_ranges.begin(), it);
    return static_cast<UnicodeBlock>(offset);
}

std::vector<UnicodeRange> unicode_ranges_for(std::u32string_view text)
{
    std::u32string codepoints{ text };
    sort_unique(codepoints);

    if (codepoints.empty()) {
        return {};
    }

    std::vector<UnicodeRange> result;
    result.push_back({ codepoints.front(), 1 });

    for (auto&& [a, b] : iterate_adjacent(codepoints)) {
        if (b == a + 1) {
            ++(result.back().length);
        }
        else {
            result.push_back({ b, 1 });
        }
    }

    return result;
}

std::vector<UnicodeRange> unicode_ranges_for(std::string_view text_utf8)
{
    return unicode_ranges_for(utf8_to_utf32(text_utf8));
}

std::vector<UnicodeRange> merge_overlapping_ranges(std::span<const UnicodeRange> unicode_ranges)
{
    auto ranges_overlap = [](const UnicodeRange& a, const UnicodeRange& b) {
        return uint32_t(a.start) + a.length >= uint32_t(b.start);
    };

    // Sort and remove duplicates.
    small_vector<UnicodeRange, 10> sorted;
    std::copy(unicode_ranges.begin(), unicode_ranges.end(), std::back_inserter(sorted));
    sort_unique(sorted);

    if (sorted.empty()) {
        return {};
    }

    // Merge ranges and write to result.
    std::vector<UnicodeRange> result;
    result.push_back(sorted.front());

    for (auto&& [a, b] : iterate_adjacent(sorted)) {
        if (ranges_overlap(a, b)) {
            const auto new_end = max(b.start + b.length, a.start + a.length);
            result.back().length = new_end - a.start;
        }
        else {
            result.push_back(b);
        }
    }

    return result;
}

// Helpers for get_unicode_codepoint_at()
namespace {

struct InitialByteInfo {
    uint8_t mask;
    uint32_t num_bytes;
};

// Get information on the initial byte of a UTF-8 codepoint sequence.
InitialByteInfo initial_byte_info(uint8_t v)
{
    // Leading 0-bit: ASCII character, one byte.
    if ((v & 0b10000000) == 0) {
        return { 0b01111111, 1 };
    }
    // Two leading ones: start of two-byte codepoint.
    if ((v & 0b11100000) == 0b11000000) {
        return { 0b00011111, 2 };
    }
    // Three leading ones: start of three-byte codepoint.
    if ((v & 0b11110000) == 0b11100000) {
        return { 0b00001111, 3 };
    }
    // Four leading ones: start of four-byte codepoint.
    if ((v & 0b11111000) == 0b11110000) {
        return { 0b00000111, 4 };
    }
    // Else not the start of a codepoint.
    return { 0, 0 };
}

// Get whether a byte is valid as a non-initial byte in a UTF-8 codepoint sequence.
bool is_non_initial_sequence_byte(uint8_t v)
{
    return ((v & 0b11000000) == 0b10000000);
}

} // namespace

CodepointResult get_unicode_codepoint_at(std::string_view utf8_string, size_t char_index)
{
    MG_ASSERT_DEBUG(char_index < utf8_string.size());
    static constexpr CodepointResult invalid_result = { 0u, 1u, false };
    constexpr uint8_t non_initial_byte_mask = 0b00111111;

    const uint8_t* c = reinterpret_cast<const uint8_t*>(&utf8_string[char_index]); // NOLINT
    const auto [initial_byte_mask, num_bytes] = initial_byte_info(*c);

    // Bounds check so that bad UTF-8 does not trick us into reading outside of buffer.
    if (num_bytes == 0 || char_index + (num_bytes - 1) >= utf8_string.size()) {
        return invalid_result;
    }

    // Trivial case for ASCII characters.
    if (num_bytes == 1u) {
        return { char32_t{ *c }, 1u, true };
    }

    // Build up codepoint. First, the bits from initial byte.
    uint32_t codepoint = 0u;
    codepoint |= static_cast<uint32_t>(*c & initial_byte_mask);

    // Then, include bits from following bytes in sequence, if any.
    uint32_t num_bytes_remaining = num_bytes - 1;
    while (num_bytes_remaining > 0) {
        c = std::next(c);
        --num_bytes_remaining;

        if (!is_non_initial_sequence_byte(*c)) {
            return invalid_result;
        }

        codepoint = codepoint << 6u;
        codepoint |= static_cast<uint32_t>(*c & non_initial_byte_mask);
    }

    return { char32_t{ codepoint }, num_bytes, true };
}

std::u32string utf8_to_utf32(std::string_view utf8_string, bool* error)
{
    std::u32string result;
    result.reserve(utf8_string.size());

    if (error) {
        *error = false;
    }

    size_t i = 0;
    while (i < utf8_string.size()) {
        const auto [codepoint, num_bytes, valid] = get_unicode_codepoint_at(utf8_string, i);

        if (valid) {
            result += codepoint;
        }
        else if (error) {
            *error = true;
        }

        i += num_bytes;
    }

    return result;
}

} // namespace Mg

//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/utils/mg_string_utils.h"

#include <algorithm>
#include <cstring> // memcpy

#include "mg/utils/mg_gsl.h"

namespace Mg {

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

bool is_whitespace(char32_t c) noexcept
{
    return is_ascii(c) &&
           std::find(k_white_space.begin(), k_white_space.end(), c) != k_white_space.end();
}
bool is_not_whitespace(char32_t c) noexcept
{
    return !is_ascii(c) &&
           std::find(k_white_space.begin(), k_white_space.end(), c) == k_white_space.end();
}

/** Tokenise string by delimiter, returns tokens in vector. */
std::vector<std::string_view> tokenise_string(std::string_view s, std::string_view delims) noexcept
{
    auto elems = std::vector<std::string_view>{};
    bool is_delim = true;
    size_t token_start = std::string::npos;
    size_t i;

    for (i = 0; i < s.length(); ++i) {
        const char c = s[i];
        const bool was_delim = is_delim;
        auto it = std::find(delims.begin(), delims.end(), c);
        is_delim = (it != delims.end());

        if (is_delim) {
            if (token_start != std::string::npos) {
                elems.push_back(s.substr(token_start, i - token_start));
                token_start = std::string::npos;
            }
        }
        else if (was_delim) {
            token_start = i;
        }
    }

    if (token_start != std::string::npos) {
        elems.push_back(s.substr(token_start, i - token_start));
    }

    return elems;
}

/** Split string on first occurrence of char c. */
std::pair<std::string_view, std::string_view> split_string_on_char(std::string_view s,
                                                                   char c) noexcept
{
    size_t index;
    for (index = 0; index < s.length(); ++index) {
        if (s[index] == c) {
            break;
        }
    }

    if (index < s.length()) {
        return { s.substr(0, index), s.substr(index + 1) };
    }

    return { s, {} };
}

/** Trim whitespace from end of string. */
std::string_view rtrim(std::string_view str) noexcept
{
    const auto end =
        std::find_if(str.rbegin(), str.rend(), [](auto c) { return is_not_whitespace(c); }).base();
    const auto length = narrow<size_t>(str.end() - end);
    str.remove_suffix(length);
    return str;
}

/** Trim whitespace from beginning of string. */
std::string_view ltrim(std::string_view str) noexcept
{
    const auto begin =
        std::find_if(str.begin(), str.end(), [](auto c) { return is_not_whitespace(c); });
    const auto length = narrow<size_t>(begin - str.begin());
    str.remove_prefix(length);
    return str;
}

/** Trim whitespace from beginning and end of string. */
std::string_view trim(std::string_view str) noexcept
{
    return ltrim(rtrim(str));
}

/** Returns the first position of any of the ASCII characters in chars. */
size_t find_any_of(std::string_view str, std::string_view chars) noexcept
{
    size_t ret_val = 0;

    for (const char c : str) {
        auto it = std::find(chars.begin(), chars.end(), c);
        if (it != chars.end()) {
            break;
        }
        ret_val++;
    }

    if (ret_val == str.length()) {
        ret_val = std::string::npos;
    }

    return ret_val;
}

/** Returns a lowercase version of an ASCII string. */
std::string to_lower(std::string_view str) noexcept
{
    std::string ret_val;
    ret_val.reserve(str.length());
    for (const char c : str) {
        ret_val += narrow<char>(tolower(c));
    }
    return ret_val;
}

/** Returns a uppercase version of an ASCII string. */
std::string to_upper(std::string_view str) noexcept
{
    std::string ret_val;
    ret_val.reserve(str.length());
    for (const char c : str) {
        ret_val += narrow<char>(toupper(c));
    }
    return ret_val;
}

} // namespace Mg

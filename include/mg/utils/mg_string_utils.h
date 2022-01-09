//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_string_utils.h
 * String handling utilities.
 */

#pragma once

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace Mg {

constexpr std::string_view k_white_space = " \t\f\v\n\r";

/** Convert char to char32_t s.t. signed/unsigned difference is handled correctly. */
constexpr char32_t to_char32(char c)
{
    return static_cast<char32_t>(static_cast<unsigned char>(c));
}

/** Get whether codepoint is an ASCII character. */
constexpr bool is_ascii(char32_t codepoint)
{
    return (codepoint & 0xFFFFFF80) == 0;
}
constexpr bool is_ascii(char codepoint)
{
    return is_ascii(to_char32(codepoint));
}

/** Is character an ASCII white-space character? */
bool is_whitespace(char32_t c) noexcept;
inline bool is_whitespace(char c) noexcept
{
    return is_whitespace(to_char32(c));
}

/** Is character not an ASCII white-space character? */
bool is_not_whitespace(char32_t c) noexcept;
inline bool is_not_whitespace(char c) noexcept
{
    return is_not_whitespace(to_char32(c));
}

constexpr bool is_ascii_digit(char32_t c) noexcept
{
    return (c >= U'0' && c <= U'9');
}
constexpr bool is_ascii_digit(char c) noexcept
{
    return is_ascii_digit(to_char32(c));
}

constexpr bool is_ascii_alphanumeric(char32_t c) noexcept
{
    return ((c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z') || c == U'_' || is_ascii_digit(c));
}
constexpr bool is_ascii_alphanumeric(char c) noexcept
{
    return is_ascii_alphanumeric(to_char32(c));
}

/** Tokenize string by delimiter, returns tokens in vector. Works on UTF-8
 * strings, but delimiter has to be ASCII.
 * @param s String to tokenize
 * @param delims String of delimiter characters
 * @return Vector of tokens. Note that these are not copies, they are references
 * to parts of the input string.
 */
std::vector<std::string_view> tokenize_string(std::string_view s, std::string_view delims) noexcept;

/** Split string on first occurrence of char c. Works on UTF-8 strings, but
 * delimiter has to be ASCII.
 * @param s The string to split
 * @param c The character on which to split.
 * @return A pair where first = substring of s before first occurrence of c,
 * second = substring after first occurrence of c. If c does not occur in s,
 * first = s, second is empty. Note that these are not copies, they are
 * references to parts of the input string.
 */
std::pair<std::string_view, std::string_view> split_string_on_char(std::string_view s,
                                                                   char c) noexcept;

/** Trim whitespace from end of string.
 * @return Substring without trailing whitespace. Note that this is not a copy,
 * it is a reference to a part of the input string.
 */
std::string_view rtrim(std::string_view str) noexcept;

/** Trim whitespace from beginning of string.
 * @return Substring without preceding whitespace. Note that this is not a copy,
 * it is a reference to a part of the input string.
 */
std::string_view ltrim(std::string_view str) noexcept;

/** Trim whitespace from beginning and end of string.
 * @return Substring without surrounding whitespace. Note that this is not a
 * copy, it is a reference to a part of the input string.
 */
std::string_view trim(std::string_view str) noexcept;

/** Returns the first position of any of the ASCII characters in chars. Works
 * for UTF-8 strings as long as the searched chars are ASCII.
 * @param str String to search in
 * @param chars Set of ASCII chars to search for.
 * @return The first position of any of the characters in chars, or
 * String::npos if none were found.
 */
size_t find_any_of(std::string_view str, std::string_view chars) noexcept;

/** Returns a lowercase version of an ASCII string. */
std::string to_lower(std::string_view str) noexcept;

/** Returns a uppercase version of an ASCII string. */
std::string to_upper(std::string_view str) noexcept;

/** Returns whether `prefix` is a prefix of `string` (including if they are equal). */
inline bool is_prefix_of(std::string_view prefix, std::string_view string)
{
    if (prefix.size() > string.size()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), string.begin());
}

/** Returns whether `suffix` is a suffix of `string` (including if they are equal). */
inline bool is_suffix_of(std::string_view suffix, std::string_view string)
{
    if (suffix.size() > string.size()) {
        return false;
    }
    return std::equal(suffix.begin(), suffix.end(), string.end() - suffix.size());
}

/** Get the substring of str up to the first occurrence of character 'c'. */
inline std::string_view substring_until(std::string_view str, const char c)
{
    const size_t index = str.find(c);
    if (index == std::string_view::npos || index == 0) {
        return std::string_view{};
    }
    return str.substr(0, index - 1);
}

/** Parse value from string if possible.
 * @param str std::string to convert
 * @return (bool success, float value), where success is true if conversion
 * succeeded. If not successful, value will be default constructed.
 */
template<typename T> std::pair<bool, T> string_to(std::string_view str)
{
    bool success = false;
    T value{};

    std::stringstream ss{ std::string(str) };

    if (ss >> value) {
        success = ss.eof();
    }

    return { success, value };
};

/** Convert numeric value to string
 * @param value Value to convert
 * @return Pair of [success, value]
 */
template<typename T> std::string string_from(const T& value) noexcept
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/** Simpler and more efficient alternative to std::istringstream for basic use-cases.
 * (Minimal interface for lexing).
 */
class SimpleInputStream {
public:
    /** Construct new stream. N.B. SimpleInputStream only references input data and does not copy,
     * hence str must outlive this stream object.
     */
    constexpr explicit SimpleInputStream(std::string_view str) noexcept : data(str) {}

    std::string_view data{};
    size_t pos = 0;
    size_t line = 1;
    size_t pos_in_line = 1;

    constexpr char advance() noexcept
    {
        if (is_at_end()) {
            return '\0';
        }

        if (data[pos] == '\n') {
            ++line;
            pos_in_line = 0;
        }

        ++pos_in_line;
        return data[pos++];
    }

    enum PeekMode { throw_on_eof, return_eof_as_null_char };
    constexpr char peek(PeekMode peekmode = throw_on_eof) const
    {
        if (is_at_end()) {
            switch (peekmode) {
            case throw_on_eof:
                log.error("Unexpected end of file.");
                throw RuntimeError{};
            case return_eof_as_null_char:
                return '\0';
            }
        }

        return data[pos];
    }

    constexpr char peek_next(size_t n = 1, PeekMode peekmode = throw_on_eof) const
    {
        if (pos + n >= data.size()) {
            switch (peekmode) {
            case throw_on_eof:
                log.error("Unexpected end of file.");
                throw RuntimeError{};
            case return_eof_as_null_char:
                return '\0';
            }
        }
        return data[pos + n];
    }

    constexpr bool match(char c, PeekMode peekmode = throw_on_eof)
    {
        if (peek(peekmode) == c) {
            advance();
            return true;
        }

        return false;
    }

    constexpr bool match(std::string_view str, PeekMode peekmode = throw_on_eof)
    {
        for (size_t i = 0; i < str.size(); ++i) {
            if (peek_next(i, peekmode) != str[i]) {
                return false;
            }
        }

        for (size_t i = 0; i < str.size(); ++i) {
            advance();
        }
        return true;
    }

    constexpr bool is_at_end() const noexcept { return pos >= data.size(); }
};

} // namespace Mg

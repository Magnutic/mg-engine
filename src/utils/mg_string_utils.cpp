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
bool is_whitespace(char32_t c) noexcept
{
    return is_ascii(c) &&
           std::find(k_white_space.begin(), k_white_space.end(), c) != k_white_space.end();
}
bool is_not_whitespace(char32_t c) noexcept
{
    return !is_ascii(c) ||
           std::find(k_white_space.begin(), k_white_space.end(), c) == k_white_space.end();
}

/** Tokenize string by delimiter, returns tokens in vector. */
std::vector<std::string_view> tokenize_string(std::string_view s, std::string_view delims) noexcept
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

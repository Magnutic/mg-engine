//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

#include "mg/utils/mg_string_utils.h"

#include <algorithm>
#include <cstring> // memcpy

#include "mg/utils/mg_gsl.h"

namespace Mg {

bool is_whitespace(char c) noexcept
{
    return std::find(k_white_space.begin(), k_white_space.end(), c) != k_white_space.end();
}
bool is_not_whitespace(char c) noexcept
{
    return std::find(k_white_space.begin(), k_white_space.end(), c) == k_white_space.end();
}

/** Tokenise string by delimiter, returns tokens in vector. */
std::vector<std::string_view> tokenise_string(std::string_view s, std::string_view delims) noexcept
{
    auto elems = std::vector<std::string_view>{};
    bool is_delim = true;
    size_t token_start = std::string::npos;
    uint32_t i;

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
    const auto end = std::find_if(str.rbegin(), str.rend(), is_not_whitespace).base();
    const auto length = narrow<size_t>(str.end() - end);
    str.remove_suffix(length);
    return str;
}

/** Trim whitespace from beginning of string. */
std::string_view ltrim(std::string_view str) noexcept
{
    const auto begin = std::find_if(str.begin(), str.end(), is_not_whitespace);
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

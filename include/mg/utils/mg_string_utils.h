//******************************************************************************
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

/** @file mg_string_utils.h
 * String handling utilities.
 */

#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace Mg {

////////////////////////////////////////////////////////////////////////////////
//                               String handling                              //
// These should only be used for internal things, as they don't handle UTF-8  //
// correctly. They do work with UTF-8 strings but non-ASCII code units are    //
// left unmodified.                                                           //
////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------
// Encoding converters
//--------------------------------------------------------------------------------------------------

/** Convert UTF-8 string to std::wstring. */
std::wstring utf8_to_wstring(std::string_view str);

/** Convert wstring to UTF-8 String. */
std::string wstring_to_utf8(std::wstring_view str);

/** Convert ISO-8859-1 (aka. LATIN_1) strings to UTF-8. */
std::string iso_8859_1_to_utf8(std::string_view str);

#ifdef _WIN32

#ifdef _MSC_VER
/** Converts string to wstring if compiled with MSVC. Intended for MSVC's file
 * stream open(std::wstring) extension. */
std::wstring widen_if_msvc(std::string_view str);

#else // Windows with some non-MSVC compiler

/* Unsolved issue here - how to open fstreams with unicode file paths on Windows? MSVC supports a
 * non-standard wstring (UTF-16) overload, but AFAIK other Windows compilers like MinGW does not.
 * After searching around a bit and reading some old mailing lists, it seems they are not interested
 * in solving this problem either.
 *
 * Until all Windows compilers support fstream::open with UTF-16 wstrings, or until Windows supports
 * UTF-8 strings in its API (even better), this problem seems to be largely unsolvable.
 *
 * UPDATE 2017: Now that filesystem is in C++17, fstream now takes filesystem::path() as a
 * parameter. I imagine MSVC will implement this to work correctly with UTF-8 paths. However, as of
 * writing, it seems libstdc++ has not yet implemented filesystem, so we will keep this solution
 * until then.
 */
#error "No support for unicode file paths in fstream on the current platform."

#endif

#else
std::string widen_if_msvc(std::string_view str);
#endif // _WIN32

/** Cross-platform (Windows, Linux, probably OS X) fopen for UTF-8 file paths.  */
std::FILE* fopen_utf8(const char* filepath_utf8, const char* mode);

//--------------------------------------------------------------------------------------------------

static const std::string_view k_white_space = " \t\f\v\n\r";

inline bool is_white_space(char c)
{
    return std::find(k_white_space.begin(), k_white_space.end(), c) != k_white_space.end();
}

inline bool is_not_whitespace(char c)
{
    return std::find(k_white_space.begin(), k_white_space.end(), c) == k_white_space.end();
}

/** Tokenise string by delimiter, returns tokens in vector. Works on UTF-8
 * strings, but delimiter has to be ASCII.
 * @param s String to tokenise
 * @param delims String of delimiter characters
 * @return Vector of tokens. Note that these are not copies, they are references
 * to parts of the input string.
 */
std::vector<std::string_view> tokenise_string(std::string_view s, std::string_view delims);

/** Split string on first occurrence of char c. Works on UTF-8 strings, but
 * delimiter has to be ASCII.
 * @param s The string to split
 * @param c The character on which to split.
 * @return A pair where first = substring of s before first occurrence of c,
 * second = substring after first occurrence of c. If c does not occur in s,
 * first = s, second is empty. Note that these are not copies, they are
 * references to parts of the input string.
 */
std::pair<std::string_view, std::string_view> split_string_on_char(std::string_view s, char c);

/** Trim whitespace from end of string.
 * @return Substring without trailing whitespace. Note that this is not a copy,
 * it is a reference to a part of the input string.
 */
std::string_view rtrim(std::string_view str);

/** Trim whitespace from beginning of string.
 * @return Substring without preceding whitespace. Note that this is not a copy,
 * it is a reference to a part of the input string.
 */
std::string_view ltrim(std::string_view str);

/** Trim whitespace from beginning and end of string.
 * @return Substring without surrounding whitespace. Note that this is not a
 * copy, it is a reference to a part of the input string.
 */
std::string_view trim(std::string_view str);

/** Returns the first position of any of the ASCII characters in chars. Works
 * for UTF-8 strings as long as the searched chars are ASCII.
 * @param str String to search in
 * @param chars Set of ASCII chars to search for.
 * @return The first position of any of the characters in chars, or
 * String::npos if none were found.
 */
size_t find_any_of(std::string_view str, std::string_view chars);

/** Returns a lowercase version of an ASCII string. */
std::string to_lower(std::string_view str);

/** Returns a uppercase version of an ASCII string. */
std::string to_upper(std::string_view str);

/** Returns a cleaned up version of a file path. Converts backslashes to
 * forward slashes, removes trailing slashes and removes duplicate slashes. */
std::string clean_path(std::string_view path);

/** Parse value from string if possible.
 * @param str std::string to convert
 * @return (bool success, float value), where success is true if conversion
 * succeeded. If not successful, value will be default constructed.
 */
template<typename T> std::pair<bool, T> string_to(std::string_view str)
{
    bool success = false;
    T    value{};

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
template<typename T> std::string string_from(const T& value)
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
    size_t           pos = 0;

    constexpr char advance() noexcept
    {
        if (is_at_end()) return '\0';
        return data[pos++];
    }

    constexpr char peek() const noexcept
    {
        if (is_at_end()) return '\0';
        return data[pos];
    }

    constexpr char peek_next() const noexcept
    {
        if (pos + 1 >= data.size()) return '\0';
        return data[pos + 1];
    }

    constexpr bool match(char c) noexcept
    {
        if (peek() == c) {
            ++pos;
            return true;
        }

        return false;
    }

    constexpr bool is_at_end() const noexcept { return pos >= data.size(); }
};

} // namespace Mg

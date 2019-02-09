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

#include <codecvt>
#include <cstring> // memcpy
#include <locale>

#include "mg/utils/mg_gsl.h"

namespace Mg {

// TODO: investigate replacements for codecvt, which is deprecated in C++17.

/** Convert UTF-8 string to wstring. */
std::wstring utf8_to_wstring(std::string_view str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv("");
    span<const char>                                 bytes{ str.data(), str.size() };
    return myconv.from_bytes(bytes.begin(), bytes.end());
}

/** Convert wstring to UTF-8 string. */
std::string wstring_to_utf8(std::wstring_view str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv("");
    span<const wchar_t>                              wchars{ str.data(), str.size() };
    auto u8str = myconv.to_bytes(wchars.begin(), wchars.end());
    return std::string{ u8str };
}

std::string iso_8859_1_to_utf8(std::string_view str)
{
    std::string out;

    for (char c : str) {
        unsigned char uc;
        std::memcpy(&uc, &c, 1);

        if (uc < 0x80) { out.push_back(c); }
        else {
            auto out_uchar0 = narrow<unsigned char>(0xc0 | uc >> 6);
            auto out_uchar1 = narrow<unsigned char>(0x80 | (uc & 0x3f));

            char out_char0, out_char1;
            std::memcpy(&out_char0, &out_uchar0, 1);
            std::memcpy(&out_char1, &out_uchar1, 1);

            out.push_back(out_char0);
            out.push_back(out_char1);
        }
    }

    return out;
}

#ifdef _MSC_VER

/** Widens a UTF-8 encoded std::string to std::wstring for file stream open()
 * MSVC extension. */
std::wstring widen_if_msvc(std::string_view str)
{
    return utf8_to_wstring(str);
}

std::FILE* fopen_utf8(const char* filepath_utf8, const char* mode)
{
    return _wfopen(utf8_to_wstring(filepath_utf8).data(), utf8_to_wstring(mode).data());
}

#else

std::string widen_if_msvc(std::string_view str)
{
    return std::string(str);
}

std::FILE* fopen_utf8(const char* filepath_utf8, const char* mode)
{
    return fopen(filepath_utf8, mode);
}

#endif // _MSC_VER

//--------------------------------------------------------------------------------------------------

/** Tokenise string by delimiter, returns tokens in vector. */
std::vector<std::string_view> tokenise_string(std::string_view s, std::string_view delims)
{
    auto     elems       = std::vector<std::string_view>{};
    bool     is_delim    = true;
    size_t   token_start = std::string::npos;
    uint32_t i;

    for (i = 0; i < s.length(); ++i) {
        char c         = s[i];
        bool was_delim = is_delim;
        auto it        = std::find(delims.begin(), delims.end(), c);
        is_delim       = (it != delims.end());

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
std::pair<std::string_view, std::string_view> split_string_on_char(std::string_view s, char c)
{
    size_t index;
    for (index = 0; index < s.length(); ++index) {
        if (s[index] == c) { break; }
    }

    if (index < s.length()) { return std::make_pair(s.substr(0, index), s.substr(index + 1)); }

    return std::make_pair(s, std::string_view{});
}

/** Trim whitespace from end of string. */
std::string_view rtrim(std::string_view str)
{
    auto end    = std::find_if(str.rbegin(), str.rend(), is_not_whitespace).base();
    auto length = size_t(str.end() - end);
    str.remove_suffix(length);
    return str;
}

/** Trim whitespace from beginning of string. */
std::string_view ltrim(std::string_view str)
{
    auto begin  = std::find_if(str.begin(), str.end(), is_not_whitespace);
    auto length = size_t(begin - str.begin());
    str.remove_prefix(length);
    return str;
}

/** Trim whitespace from beginning and end of string. */
std::string_view trim(std::string_view str)
{
    return ltrim(rtrim(str));
}

/** Returns the first position of any of the ASCII characters in chars. */
size_t find_any_of(std::string_view str, std::string_view chars)
{
    size_t ret_val = 0;

    for (char c : str) {
        auto it = std::find(chars.begin(), chars.end(), c);
        if (it != chars.end()) { break; }
        ret_val++;
    }

    if (ret_val == str.length()) { ret_val = std::string::npos; }

    return ret_val;
}

/** Returns a lowercase version of an ASCII string. */
std::string to_lower(std::string_view str)
{
    std::string ret_val;
    ret_val.reserve(str.length());
    for (char c : str) { ret_val += static_cast<char>(tolower(c)); }
    return ret_val;
}

/** Returns a uppercase version of an ASCII string. */
std::string to_upper(std::string_view str)
{
    std::string ret_val;
    ret_val.reserve(str.length());
    for (char c : str) { ret_val += static_cast<char>(toupper(c)); }
    return ret_val;
}

/** Returns a clean version of a file path. */
std::string clean_path(std::string_view path)
{
    std::string ret_val(path);

    std::replace(ret_val.begin(), ret_val.end(), '\\', '/');

    for (;;) {
        auto duplicate = std::search_n(ret_val.begin(), ret_val.end(), 2, '/');
        if (duplicate == ret_val.end()) { break; }
        ret_val.erase(duplicate);
    }

    auto last_index = ret_val.find_last_not_of('/');

    if (last_index != std::string::npos) { ret_val = ret_val.substr(0, last_index + 1); }

    return ret_val;
}

} // namespace Mg

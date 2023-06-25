//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_u8string_casts.h
 * Convert std::u8string and std::u8string_view to and from regular string and string view
 * containing the same UTF-8 data. No transcoding take place; all strings -- input and output -- are
 * expected to be UTF-8.
 *
 * C++20 added the char8_t type, along with std::u8string and std::u8string_view. This causes
 * friction as some newer standard-library functionality works with the new types that are
 * explicitly UTF-8, while most interfaces (including much of the standard library) still use the
 * old char, std::string, and std::string_view types. For this reason, Mg Engine uses std::string
 * and std::string_view containing UTF-8 data. The functions in this header are used to convert to
 * and from the new types at API boundaries.
 */

#pragma once

#include <cstring>
#include <string>

namespace Mg {

inline std::string cast_u8_to_char(std::u8string_view str)
{
    return { reinterpret_cast<const char*>(str.data()), str.size() };
}

inline std::u8string cast_as_u8_unchecked(std::string_view str)
{
    std::u8string result;
    result.resize(str.size());
    std::memcpy(result.data(), str.data(), str.size());
    return result;
}

} // namespace Mg

//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_u8string_to_string.h
 * Convert std::u8string and std::u8string_view to regular string and string view containing the
 * same UTF-8 data.
 */

#pragma once

#include <cstring>
#include <string>

namespace Mg {

inline std::string u8string_to_string(std::u8string str)
{
    std::string result;
    result.resize(str.size());
    std::memcpy(result.data(), str.data(), str.size());
    return result;
}

inline std::string u8string_to_string(std::u8string_view str)
{
    std::string result;
    result.resize(str.size());
    std::memcpy(result.data(), str.data(), str.size());
    return result;
}

inline std::string_view u8string_view_to_string_view(std::u8string_view str)
{
    return { reinterpret_cast<const char*>(str.data()), str.size() };
}

} // namespace Mg

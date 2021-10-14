//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_vec_to_string.h
 * Function for converting glm vector types to a string representation.
 */

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string>

namespace Mg {

inline std::string
to_string(const glm::vec2& v, bool include_type_name = true, bool use_braces = false)
{
    const auto begin = std::string(include_type_name ? "vec2" : "") + (use_braces ? '{' : '(');
    const auto end = use_braces ? '}' : ')';
    return begin + std::to_string(v.x) + ", " + std::to_string(v.y) + end;
}

inline std::string
to_string(const glm::vec3& v, bool include_type_name = true, bool use_braces = false)
{
    const auto begin = std::string(include_type_name ? "vec3" : "") + (use_braces ? '{' : '(');
    const auto end = use_braces ? '}' : ')';
    return begin + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) +
           end;
}

inline std::string
to_string(const glm::vec4& v, bool include_type_name = true, bool use_braces = false)
{
    const auto begin = std::string(include_type_name ? "vec4" : "") + (use_braces ? '{' : '(');
    const auto end = use_braces ? '}' : ')';
    return begin + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) +
           ", " + std::to_string(v.w) + end;
}

} // namespace Mg

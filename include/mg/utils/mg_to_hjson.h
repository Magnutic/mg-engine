//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file utils/mg_to_hjson.h
 * Convert values to Hjson::Value.
 */

#pragma once

#include <hjson/hjson.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <concepts>

namespace Mg {

/** Convert value to Hjson::Value. Overload for types from which Hjson::Value is constructible. */
template<std::convertible_to<Hjson::Value> T> Hjson::Value to_hjson(T value)
{
    return Hjson::Value(value);
}

inline Hjson::Value to_hjson(glm::vec2 v)
{
    Hjson::Value result(Hjson::Type::Vector);
    result.push_back(v[0]);
    result.push_back(v[1]);
    return result;
}

inline Hjson::Value to_hjson(glm::vec3 v)
{
    Hjson::Value result(Hjson::Type::Vector);
    result.push_back(v[0]);
    result.push_back(v[1]);
    result.push_back(v[2]);
    return result;
}

inline Hjson::Value to_hjson(glm::vec4 v)
{
    Hjson::Value result(Hjson::Type::Vector);
    result.push_back(v[0]);
    result.push_back(v[1]);
    result.push_back(v[2]);
    result.push_back(v[3]);
    return result;
}

} // namespace Mg

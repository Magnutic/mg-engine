//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_interpolation.h
 * Value interpolation functions.
 */

#pragma once

namespace Mg {

/** Linear interpolation between edge0 and edge1 by factor x. */
template<typename T> constexpr T lerp(const T& edge0, const T& edge1, float x)
{
    if (x <= 0.0f) return edge0;
    if (x >= 1.0f) return edge1;
    return (1.0f - x) * edge0 + x * edge1;
}

/** Interpolation between edge0 and edge1 by factor x, starting slow then accelerating. */
template<typename T> constexpr T ease_in(const T& edge0, const T& edge1, float x)
{
    if (x <= 0.0f) return edge0;
    if (x >= 1.0f) return edge1;
    const auto t = x * x;
    return (1.0f - t) * edge0 + t * edge1;
}

/** Interpolation between edge0 and edge1 by factor x, starting fast then decelerating. */
template<typename T> constexpr T ease_out(const T& edge0, const T& edge1, float x)
{
    if (x <= 0.0f) return edge0;
    if (x >= 1.0f) return edge1;
    const auto t = 1.0f - (1.0f - x) * (1.0f - x);
    return (1.0f - t) * edge0 + t * edge1;
}

/** Smoothstep interpolation between edge0 and edge1 by factor x. */
template<typename T> constexpr T smoothstep(const T& edge0, const T& edge1, float x)
{
    if (x <= 0.0f) return edge0;
    if (x >= 1.0f) return edge1;
    const auto t = x * x * (3.0f - 2.0f * x);
    return (1.0f - t) * edge0 + t * edge1;
}

} // namespace Mg

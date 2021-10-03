//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_utils.h
 * Several small math utilities.
 */

#pragma once

#include <cmath>
#include <type_traits>

#include "mg/utils/mg_gsl.h"

namespace Mg {

namespace detail {

template<typename UnsignedT>
constexpr UnsignedT sign(UnsignedT val, std::false_type /* is_signed */)
{
    return UnsignedT{} < val;
}

template<typename SignedT> constexpr SignedT sign(SignedT val, std::true_type /* is_signed */)
{
    return (SignedT{} < val) - (val < SignedT{});
}

} // namespace detail

/** Returns the sign of val. */
template<typename NumT> constexpr NumT sign(NumT val)
{
    static_assert(std::is_arithmetic_v<NumT>, "sign() cannot be used on non-arithmetic type");
    return detail::sign(val, std::is_signed<NumT>{});
}

/** Round to nearest integer. */
template<typename IntT, typename FloatT> constexpr IntT round(FloatT value)
{
    static_assert(std::is_integral_v<IntT>, "IntT must be integral type.");
    static_assert(std::is_floating_point_v<FloatT>, "FloatT must be floating point type.");
    return gsl::narrow<IntT>(std::lround(value));
}

/** Integral constexpr power. */
template<typename T> constexpr T intpow(const T base, unsigned const exponent)
{
    return (exponent == 0) ? 1 : base * std::pow(base, exponent - 1);
}

/** Absolute value. */
template<typename T> constexpr T abs(T val)
{
    return (val < 0) ? -val : val;
}

/** Equivalent to std::max. Re-implemented here because std::max resides in the very heavy
 * <algorithm> header, which is too much to include for such a small function.
 */
template<typename T> constexpr const T& max(const T& l, const T& r) noexcept
{
    return l > r ? l : r;
}

/** Equivalent to std::min. Re-implemented here because std::max resides in the very heavy
 * <algorithm> header, which is too much to include for such a small function.
 */
template<typename T> constexpr const T& min(const T& l, const T& r) noexcept
{
    return l > r ? r : l;
}

/** Equivalent to std::clamp. Re-implemented here because std::clamp resides in the very heavy
 * <algorithm> header, which is too much to include for such a small function.
 */
template<typename T> constexpr const T& clamp(const T& x, const T& low, const T& high) noexcept
{
    return x < low ? low : x > high ? high : x;
}

/** Cubic bezier curve / interpolation. The curve is defined by the four control points p0, p1, p2,
 * p3. `t` is the length along to curve at which to evaluate, such that `t` is in [0.0f, 1.0f].
 * @return The position along the curve at `t`.
 */
template<typename T>
inline T cubic_bezier(const T& p0, const T& p1, const T& p2, const T& p3, float t)
{
    const float one_minus_t = (1.0f - t);
    const float one_minus_t_sqr = one_minus_t * one_minus_t;
    const float one_minus_t_cube = one_minus_t * one_minus_t * one_minus_t;
    const float t_sqr = t * t;
    const float t_cube = t * t * t;
    return (one_minus_t_cube * p0) + (3 * one_minus_t_sqr * t * p1) +
           (3 * one_minus_t * t_sqr * p2) + (t_cube * p3);
}

} // namespace Mg

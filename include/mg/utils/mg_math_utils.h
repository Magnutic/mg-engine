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

/** @file mg_utils.h
 * Several small math utilities.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "mg/utils/mg_gsl.h"

namespace Mg {


////////////////////////////////////////////////////////////////////////////////
//                                Math utilities                              //
////////////////////////////////////////////////////////////////////////////////

namespace detail {

template<typename UNumT> constexpr int sign(UNumT val, std::false_type /* is_signed */)
{
    return UNumT{ 0u } < val;
}

template<typename SNumT> constexpr int sign(SNumT val, std::true_type /* is_signed */)
{
    return (SNumT{ 0 } < val) - (val < SNumT{ 0 });
}

} // namespace detail

/** Returns the sign of val. */
template<typename NumT> constexpr int sign(NumT val)
{
    static_assert(std::is_arithmetic<NumT>::value, "sign() cannot be used on non-arithmetic type");
    return detail::sign(val, std::is_signed<NumT>{});
}

/** Round to nearest integer. */
template<typename IntT, typename FloatT> constexpr IntT round(FloatT value)
{
    static_assert(std::is_integral<IntT>::value, "IntT must be integral type.");
    static_assert(std::is_floating_point<FloatT>::value, "FloatT must be floating point type.");
    return gsl::narrow<IntT>(std::lround(value));
}

/** Integral constexpr power. */
template<typename T> constexpr T pow(const T base, unsigned const exponent)
{
    return (exponent == 0) ? 1 : base * pow(base, exponent - 1);
}

/** Absolute value. */
template<typename T> constexpr T abs(T val)
{
    return (val < 0) ? -val : val;
}

/** Plane in 3D space in point-normal form. */
class PointNormalPlane {
public:
    /** Create a plane given a point on the plane and the plane's normal vector. */
    static PointNormalPlane from_point_and_normal(glm::vec3 point, glm::vec3 normal)
    {
        normal = glm::normalize(normal);
        return { normal.x,
                 normal.y,
                 normal.z,
                 -normal.x * point.x - normal.y * point.y - normal.z * point.z };
    }

    // Add more constructors as needed.
    // Members are private to maintain a,b,c normal being normalised as an invariant.

private:
    PointNormalPlane() = default;

    PointNormalPlane(float a_, float b_, float c_, float d_) noexcept : a(a_), b(b_), c(c_), d(d_)
    {}

    /** Signed shortest distance (i.e. negative if on the side of the plane facing away from the
     * plane's normal) from plane to point in 3D space.
     */
    friend constexpr float signed_distance_to_plane(PointNormalPlane plane,
                                                    glm::vec3 point) noexcept
    {
        return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
    }

    float a;
    float b;
    float c;
    float d;
};


/** Shortest distance from plane to point in 3D space. */
inline constexpr float distance_to_plane(PointNormalPlane plane, glm::vec3 point)
{
    return abs(signed_distance_to_plane(plane, point));
}

} // namespace Mg

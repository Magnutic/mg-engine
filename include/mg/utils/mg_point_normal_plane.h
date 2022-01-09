//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_point_normal_plane.h
 * Plane in 3D space in point-normal form.
 */

#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>

namespace Mg {

/** Plane in 3D space in point-normal form. */
class PointNormalPlane {
public:
    /** Create a plane given a point on the plane and the plane's normal vector. */
    static PointNormalPlane from_point_and_normal(glm::vec3 point, glm::vec3 normal)
    {
        normal = glm::normalize(normal);
        return PointNormalPlane{ normal.x,
                                 normal.y,
                                 normal.z,
                                 -normal.x * point.x - normal.y * point.y - normal.z * point.z };
    }

    /** Create a plane given the four coefficients A, B, C, and D. */
    static PointNormalPlane from_coefficients(const glm::vec4& coefficients)
    {
        const float normal_magnitude = glm::length(glm::vec3(coefficients));
        const glm::vec4 plane = coefficients / normal_magnitude;
        return PointNormalPlane{ plane.x, plane.y, plane.z, plane.w };
    }


    /** Same as from_coefficients, but assumes that the coefficients are already normalized (i.e.
     * length of (a,b,c) == 1.
     */
    static PointNormalPlane from_normalized_coefficients(const glm::vec4& coefficients)
    {
        return PointNormalPlane{ coefficients.x, coefficients.y, coefficients.z, coefficients.w };
    }

    PointNormalPlane() = default;

    /** Signed shortest distance (i.e. negative if on the side of the plane facing away from the
     * plane's normal) from plane to point in 3D space.
     */
    friend constexpr float signed_distance_to_plane(PointNormalPlane plane,
                                                    glm::vec3 point) noexcept
    {
        return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
    }

private:
    explicit PointNormalPlane(float a_, float b_, float c_, float d_) noexcept
        : a(a_), b(b_), c(c_), d(d_)
    {}

    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 0.0f;
};


/** Shortest distance from plane to point in 3D space. */
inline constexpr float distance_to_plane(PointNormalPlane plane, glm::vec3 point)
{
    return std::abs(signed_distance_to_plane(plane, point));
}

} // namespace Mg

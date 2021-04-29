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

    // Add more constructors as needed.
    // Members are private to maintain a,b,c normal being normalised as an invariant.

private:
    PointNormalPlane() = default;

    explicit PointNormalPlane(float a_, float b_, float c_, float d_) noexcept
        : a(a_), b(b_), c(c_), d(d_)
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
    return std::abs(signed_distance_to_plane(plane, point));
}

} // namespace Mg

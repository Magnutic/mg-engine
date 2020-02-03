//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_frustum.h
 * Frustum culling functionality.
 */

#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#    define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/fast_square_root.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mg::gfx {

/** Returns true if object at given clip-space coords and radius are outside the camera frustum
 * described by the MVP matrix.
 */
inline bool frustum_cull(const glm::mat4& mvp, const glm::vec3 centre, float radius)
{
    glm::vec4 plane;

    const auto cull = [&]() {
        const float magnitude = glm::fastSqrt(plane.x * plane.x + plane.y * plane.y +
                                              plane.z * plane.z);

        plane /= magnitude;

        const float distance = plane.x * centre.x + plane.y * centre.y + plane.z * centre.z +
                               plane.w;

        return distance <= -radius;
    };

    // Right
    plane = {
        mvp[0][3] + mvp[0][0], mvp[1][3] + mvp[1][0], mvp[2][3] + mvp[2][0], mvp[3][3] + mvp[3][0]
    };

    if (cull()) {
        return true;
    }

    // Left
    plane = {
        mvp[0][3] - mvp[0][0], mvp[1][3] - mvp[1][0], mvp[2][3] - mvp[2][0], mvp[3][3] - mvp[3][0]
    };

    if (cull()) {
        return true;
    }

    // Bottom
    plane = {
        mvp[0][3] + mvp[0][1], mvp[1][3] + mvp[1][1], mvp[2][3] + mvp[2][1], mvp[3][3] + mvp[3][1]
    };

    if (cull()) {
        return true;
    }

    // Top
    plane = {
        mvp[0][3] - mvp[0][1], mvp[1][3] - mvp[1][1], mvp[2][3] - mvp[2][1], mvp[3][3] - mvp[3][1]
    };

    if (cull()) {
        return true;
    }

    // Near
    plane = {
        mvp[0][3] + mvp[0][2], mvp[1][3] + mvp[1][2], mvp[2][3] + mvp[2][2], mvp[3][3] + mvp[3][2]
    };

    if (cull()) {
        return true;
    }

    // Far
    plane = {
        mvp[0][3] - mvp[0][2], mvp[1][3] - mvp[1][2], mvp[2][3] - mvp[2][2], mvp[3][3] - mvp[3][2]
    };

    return cull();
}

} // namespace Mg::gfx

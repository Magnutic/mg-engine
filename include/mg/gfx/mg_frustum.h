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

/** @file mg_frustum.h
 * Frustum culling functionality.
 */

#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
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

    auto cull = [&]() {
        float magnitude = glm::fastSqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);

        plane /= magnitude;

        float distance = plane.x * centre.x + plane.y * centre.y + plane.z * centre.z + plane.w;

        return distance <= -radius;
    };

    // Right
    plane = { mvp[0][3] + mvp[0][0],
              mvp[1][3] + mvp[1][0],
              mvp[2][3] + mvp[2][0],
              mvp[3][3] + mvp[3][0] };

    if (cull()) { return true; }

    // Left
    plane = { mvp[0][3] - mvp[0][0],
              mvp[1][3] - mvp[1][0],
              mvp[2][3] - mvp[2][0],
              mvp[3][3] - mvp[3][0] };

    if (cull()) { return true; }

    // Bottom
    plane = { mvp[0][3] + mvp[0][1],
              mvp[1][3] + mvp[1][1],
              mvp[2][3] + mvp[2][1],
              mvp[3][3] + mvp[3][1] };

    if (cull()) { return true; }

    // Top
    plane = { mvp[0][3] - mvp[0][1],
              mvp[1][3] - mvp[1][1],
              mvp[2][3] - mvp[2][1],
              mvp[3][3] - mvp[3][1] };

    if (cull()) { return true; }

    // Near
    plane = { mvp[0][3] + mvp[0][2],
              mvp[1][3] + mvp[1][2],
              mvp[2][3] + mvp[2][2],
              mvp[3][3] + mvp[3][2] };

    if (cull()) { return true; }

    // Far
    plane = { mvp[0][3] - mvp[0][2],
              mvp[1][3] - mvp[1][2],
              mvp[2][3] - mvp[2][2],
              mvp[3][3] - mvp[3][2] };

    return cull();
}

} // namespace Mg::gfx

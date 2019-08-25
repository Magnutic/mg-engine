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

// Rotation class implementation
// Parts of this code was adapted from:
// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/

#include "mg/core/mg_rotation.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace Mg {

Rotation Rotation::rotation_between_vectors(glm::vec3 fst, glm::vec3 snd)
{
    fst = glm::normalize(fst);
    snd = glm::normalize(snd);

    float     cos_theta = glm::dot(fst, snd);
    glm::vec3 rotation_axis;

    if (cos_theta < -1.0f + 0.001f) {
        // Special case when vectors in opposite directions:
        // There is no ideal rotation axis, so guess one;
        // any will do as long as it's perpendicular to fst
        rotation_axis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), fst);

        if (glm::length2(rotation_axis) < 0.01f) {
            // They were parallel, try again
            rotation_axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), fst);
        }

        rotation_axis = glm::normalize(rotation_axis);
        return Rotation{ glm::angleAxis(180.0f, rotation_axis) };
    }

    rotation_axis = cross(fst, snd);

    float s    = std::sqrt((1.0f + cos_theta) * 2.0f);
    float invs = 1.0f / s;

    return Rotation{ glm::quat{
        s * 0.5f, rotation_axis.x * invs, rotation_axis.y * invs, rotation_axis.z * invs } };
}

Rotation Rotation::look_to(glm::vec3 dir, glm::vec3 up)
{
    dir = normalize(dir);
    up  = normalize(up);

    Rotation rot = rotation_between_vectors(world_vector::forward, dir);

    glm::vec3 current_up = rot.up();
    glm::vec3 desired_up = glm::normalize(glm::cross(glm::cross(dir, up), dir));

    float angle = glm::orientedAngle(current_up, desired_up, dir);

    glm::quat up_adjustment = glm::angleAxis(angle, dir);

    return Rotation{ up_adjustment * rot.m_quaternion };
}

} // namespace Mg

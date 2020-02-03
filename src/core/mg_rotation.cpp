//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

// Rotation class implementation
// Parts of this code was adapted from:
// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/

#include "mg/core/mg_rotation.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace Mg {

Rotation Rotation::rotation_between_vectors(glm::vec3 fst, glm::vec3 snd) noexcept
{
    fst = glm::normalize(fst);
    snd = glm::normalize(snd);

    const float cos_theta = glm::dot(fst, snd);
    glm::vec3 rotation_axis{};

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

    const float s = std::sqrt((1.0f + cos_theta) * 2.0f);
    const float invs = 1.0f / s;

    return Rotation{ glm::quat{
        s * 0.5f, rotation_axis.x * invs, rotation_axis.y * invs, rotation_axis.z * invs } };
}

Rotation Rotation::look_to(glm::vec3 dir, glm::vec3 up) noexcept
{
    dir = normalize(dir);
    up = normalize(up);

    const Rotation rot = rotation_between_vectors(world_vector::forward, dir);

    const glm::vec3 current_up = rot.up();
    const glm::vec3 desired_up = glm::normalize(glm::cross(glm::cross(dir, up), dir));

    const float angle = glm::orientedAngle(current_up, desired_up, dir);

    const glm::quat up_adjustment = glm::angleAxis(angle, dir);

    return Rotation{ up_adjustment * rot.m_quaternion };
}

} // namespace Mg

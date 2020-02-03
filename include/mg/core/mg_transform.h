//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_transform.h
 * Transform class: Scale, Rotation and Position.
 */

#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#    define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "mg/core/mg_rotation.h"

namespace Mg {

class Transform {
public:
    Transform(glm::vec3 _position = glm::vec3{ 0.0f },
              glm::vec3 _scale = glm::vec3{ 1.0f },
              Rotation _rotation = Rotation{}) noexcept
        : position(_position), scale(_scale), rotation(_rotation)
    {}

    glm::vec3 position;
    glm::vec3 scale;
    Rotation rotation;

    /** Gets the transformation matrix for this transform. */
    glm::mat4 matrix() const
    {
        return glm::translate(position) * rotation.to_matrix() * glm::scale(scale);
    }

    /** Sets the rotation so the forward vector faces target. */
    void look_at(glm::vec3 target, glm::vec3 up = world_vector::up) const
    {
        rotation.look_to(target - position, up);
    }
};

} // namespace Mg

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

/** @file mg_transform.h
 * Transform class: Scale, Rotation and Position.
 */

#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "mg/core/mg_rotation.h"

namespace Mg {

class Transform {
public:
    Transform(glm::vec3 _position = glm::vec3{ 0.0f },
              glm::vec3 _scale    = glm::vec3{ 1.0f },
              Rotation  _rotation = Rotation{})
        : position(_position), scale(_scale), rotation(_rotation)
    {}

    glm::vec3 position;
    glm::vec3 scale;
    Rotation  rotation;

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

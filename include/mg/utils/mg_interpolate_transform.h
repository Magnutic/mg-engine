//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_interpolate_transform.h
 * Interpolation between transformation matrices.
 */

#pragma once

#include "mg/core/mg_rotation.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Mg {

struct DecomposedTransform {
    glm::vec4 position;
    Rotation rotation;
};

// Assumes m has no shearing and no scaling.
inline DecomposedTransform decompose(const glm::mat4& m)
{
    return { m[3], Rotation{ quat_cast(glm::mat3(m)) } };
}

inline glm::mat4
interpolate_transforms(const glm::mat4& lhs, const glm::mat4& rhs, const float factor)
{
    const auto l = decompose(lhs);
    const auto r = decompose(rhs);
    glm::mat4 result = Rotation::mix(l.rotation, r.rotation, factor).to_matrix();
    result[3] = mix(l.position, r.position, factor);
    return result;
}

} // namespace Mg

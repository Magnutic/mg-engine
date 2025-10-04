//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_bullet_utils.h
 * .
 */

#pragma once

#include "mg/core/mg_rotation.h"
#include "mg/physics/mg_collision_group.h"

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <LinearMath/btTransform.h>

#include <cstring>

namespace Mg::physics {

// Conversion functions for vectors and matrices
// These compile down to just a few instructions in release builds (verified with clang-11).

inline btTransform convert_transform(const glm::mat4& transform)
{
    btTransform result;
    result.setFromOpenGLMatrix(&transform[0][0]);
    return result;
}

inline glm::mat4 convert_transform(const btTransform& transform)
{
    glm::mat4 result;
    transform.getOpenGLMatrix(&result[0][0]);
    return result;
}

inline btVector3 convert_vector(const glm::vec3& vector)
{
    return { vector.x, vector.y, vector.z };
}

inline glm::vec3 convert_vector(const btVector3& vector)
{
    return { vector.x(), vector.y(), vector.z() };
}

inline int convert_collision_group(const CollisionGroup group)
{
    int result{};
    static_assert(sizeof(result) == sizeof(group));
    std::memcpy(&result, &group, sizeof(group));
    return result;
}

inline CollisionGroup convert_collision_group(const int group)
{
    CollisionGroup result{};
    static_assert(sizeof(result) == sizeof(group));
    std::memcpy(&result, &group, sizeof(group));
    return result;
}

//--------------------------------------------------------------------------------------------------
// Decompose matrices, for interpolation

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

} // namespace Mg::physics

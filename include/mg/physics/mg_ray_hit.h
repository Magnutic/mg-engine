//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_ray_hit.h
 * .
 */

#pragma once

#include "mg/physics/mg_physics_body_handle.h"

namespace Mg::physics {

/** Result of raycast or convex sweep. */
struct RayHit {
    /** The body that was hit by the ray or sweep. */
    PhysicsBodyHandle body;

    /** Position in world space at which the ray hit the body. */
    glm::vec3 hit_point_worldspace = { 0.0f, 0.0f, 0.0f };

    /** Normal vector in world space on the body where the ray hit. */
    glm::vec3 hit_normal_worldspace = { 0.0f, 0.0f, 0.0f };

    /** Fraction of distance between ray start and end where the ray hit `body`. */
    float hit_fraction = 0.0f;
};

} // namespace Mg::physics

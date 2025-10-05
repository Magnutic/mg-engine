//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_collision.h
 * .
 */

#pragma once

#include "mg/core/physics/mg_physics_body_handle.h"

namespace Mg::physics {

struct Collision {
    /** Handle of the first object involved in the collision. */
    PhysicsBodyHandle object_a;

    /** Handle of the second object involved in the collision. */
    PhysicsBodyHandle object_b;

    /** Contact point, in world space, on the first object. */
    glm::vec3 contact_point_on_a = glm::vec3(0.0f);

    /** Contact point, in world space, on the second object. */
    glm::vec3 contact_point_on_b = glm::vec3(0.0f);

    glm::vec3 normal_on_b = glm::vec3(0.0f);

    /** The impulse that was applied as a result of the collision. */
    float applied_impulse = 0.0f;

    /** The distance between the contact points. TODO: understand and document this better. To my
     * understanding, it is signed, with negative distance meaning that the bodies penetrate each
     * other.
     */
    float distance = 0.0f;
};

} // namespace Mg::physics

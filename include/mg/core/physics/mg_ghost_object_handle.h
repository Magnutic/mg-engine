//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_ghost_object_handle.h
 * .
 */

#pragma once

#include "mg/core/physics/mg_physics_body_handle.h"
#include "mg/utils/mg_assert.h"

#include <span>

namespace Mg::physics {

struct Collision;

/** Handle to a GhostObject. Lifetime of the referee is automatically managed via reference
 * counting.
 */
class GhostObjectHandle : public PhysicsBodyHandle {
public:
    GhostObjectHandle() = default;

    static GhostObjectHandle downcast(const PhysicsBodyHandle& handle)
    {
        MG_ASSERT(handle.type() == PhysicsBodyType::ghost_object);
        return GhostObjectHandle(handle.m_data);
    }

    void set_transform(const glm::mat4& transform);

    void set_position(const glm::vec3& position)
    {
        auto temp = get_transform();
        temp[3] = glm::vec4(position, 1.0f);
        set_transform(temp);
    }

    /** Get all collisions involving this object during the most recent update.
     * Pointers remain valid until the next call to Mg::physics::World::update().
     */
    std::span<const Collision* const> collisions() const;

private:
    explicit GhostObjectHandle(PhysicsBody* data) : PhysicsBodyHandle(data) {}

    GhostObject& data();
    const GhostObject& data() const;
};


} // namespace Mg::physics

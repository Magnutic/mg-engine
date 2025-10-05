//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#pragma once

#include "mg/core/physics/mg_ghost_object_handle.h"

#include "mg_bullet_utils.h"
#include "mg_physics_body.h"
#include "mg_shape_base.h"

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

namespace Mg::physics {

class GhostObject : public PhysicsBody {
public:
    // Construct a ghost body using the supplied collision shape and parameters.
    explicit GhostObject(const Identifier& id_, ShapeBase& shape_, const glm::mat4 transform_)
        : PhysicsBody(PhysicsBodyType::ghost_object, id_, shape_, transform_)
    {
        object.setUserPointer(this);
        object.setWorldTransform(convert_transform(transform_));
        object.setCollisionShape(&(shape_base_cast(shape_).bullet_shape()));
        object.setActivationState(DISABLE_DEACTIVATION);
    }

    btCollisionObject& get_bt_body() override { return object; }
    const btCollisionObject& get_bt_body() const override { return object; }

    btPairCachingGhostObject object;

    // The collisions involving this object in the most recent update. See
    // Mg::physics::World::update.
    std::vector<const Collision*> collisions;
};

} // namespace Mg::physics

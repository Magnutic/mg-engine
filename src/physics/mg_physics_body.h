//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_physics_body.h
 * .
 */

#pragma once

#include "mg/physics/mg_physics_body_handle.h"

#include "mg_shape_base.h"

#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

namespace Mg::physics {

// Common base for all physics bodies.
class PhysicsBody {
public:
    explicit PhysicsBody(const PhysicsBodyType type_,
                         const Identifier& id_,
                         ShapeBase& shape_,
                         const glm::mat4& transform_)
        : type(type_), id(id_), transform(transform_), shape(&shape_)
    {
        ++shape->ref_count;
    }

    virtual ~PhysicsBody() = default;

    MG_MAKE_NON_COPYABLE(PhysicsBody);
    MG_MAKE_NON_MOVABLE(PhysicsBody);

    virtual btCollisionObject& get_bt_body() = 0;
    virtual const btCollisionObject& get_bt_body() const = 0;

    PhysicsBodyType type;
    Identifier id;
    glm::mat4 transform;
    ShapeBase* shape;

    // How many handles to this body are there?
    std::atomic_int ref_count = 0;
};

[[maybe_unused]] inline PhysicsBody* collision_object_cast(const btCollisionObject* bullet_object)
{
    return static_cast<PhysicsBody*>(bullet_object->getUserPointer());
}

} // namespace Mg::physics

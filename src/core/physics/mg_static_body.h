//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#pragma once

#include "mg/core/physics/mg_static_body_handle.h"

#include "mg_bullet_utils.h"
#include "mg_physics_body.h"
#include "mg_shape_base.h"

namespace Mg::physics {

class StaticBody : public PhysicsBody {
public:
    // Construct a static body using the supplied collision shape and parameters.
    explicit StaticBody(const Identifier& id_, ShapeBase& shape_, const glm::mat4 transform_)
        : PhysicsBody(PhysicsBodyType::static_body, id_, shape_, transform_)
    {
        body.setCollisionShape(&shape_.bullet_shape());
        body.setWorldTransform(convert_transform(transform_));
        body.setUserPointer(this);
        body.setCollisionFlags(body.getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

        // FIXME: configurable friction
        body.setFriction(0.5f);
    }

    btCollisionObject& get_bt_body() override { return body; }
    const btCollisionObject& get_bt_body() const override { return body; }

    // The bullet collision object.
    btCollisionObject body;
};

} // namespace Mg::physics
